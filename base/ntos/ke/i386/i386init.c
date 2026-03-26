#include <stdint.h>

/*
 * Minimal KE/i386 processor bring-up for NaznaOS v0.0.0.4
 *
 * - Installs a private GDT with:
 *     * KGDT_R0_CODE (0x08)
 *     * KGDT_R0_DATA (0x10)
 *     * KGDT_TSS     (0x28)
 *     * KGDT_R0_PCR  (0x30)
 * - Sets up a minimal TSS with a dedicated ring0 stack
 * - Sets up a minimal PCR structure and maps it via FS
 * - Installs a flat IDT with a default handler for all vectors
 *
 * This is a clean C re-implementation inspired by your old ks386/i386pcr layout,
 * but not binary-compatible with the original NT-style PCR yet.
 */

#define KGDT_R0_CODE 0x08
#define KGDT_R0_DATA 0x10
#define KGDT_TSS     0x28
#define KGDT_R0_PCR  0x30

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint16_t es;
    uint16_t reserved0;
    uint16_t cs;
    uint16_t reserved1;
    uint16_t ss;
    uint16_t reserved2;
    uint16_t ds;
    uint16_t reserved3;
    uint16_t fs;
    uint16_t reserved4;
    uint16_t gs;
    uint16_t reserved5;
    uint16_t ldt;
    uint16_t reserved6;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

struct pcr_minimal {
    struct pcr_minimal *Self;
    void *Idt;
    void *Gdt;
    void *Tss;
};

static struct idt_entry Idt[256];
static struct idt_ptr   IdtDescriptor;

static struct gdt_entry Gdt[7];
static struct gdt_ptr   GdtDescriptor;

static struct tss_entry Tss;
static uint8_t          TssStack[4096];

static struct pcr_minimal Pcr;

/* ---------------------------------------------------------------------- */
/* IDT: default handler and setup                                         */
/* ---------------------------------------------------------------------- */

__attribute__((noreturn))
static void KeDefaultInterruptHandler(void) {
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

static void KeSetIdtEntry(int vec, void (*handler)(void)) {
    uint32_t base = (uint32_t)handler;

    Idt[vec].base_low  = (uint16_t)(base & 0xFFFF);
    Idt[vec].sel       = KGDT_R0_CODE;
    Idt[vec].always0   = 0;
    Idt[vec].flags     = 0x8E; /* present, ring0, 32-bit interrupt gate */
    Idt[vec].base_high = (uint16_t)((base >> 16) & 0xFFFF);
}

static void KeLoadIdt(void) {
    __asm__ __volatile__("lidtl (%0)" : : "r" (&IdtDescriptor));
}

static void KeInitIdt(void) {
    for (int i = 0; i < 256; i++) {
        Idt[i].base_low  = 0;
        Idt[i].sel       = 0;
        Idt[i].always0   = 0;
        Idt[i].flags     = 0;
        Idt[i].base_high = 0;
    }

    for (int i = 0; i < 256; i++) {
        KeSetIdtEntry(i, KeDefaultInterruptHandler);
    }

    IdtDescriptor.limit = (uint16_t)(sizeof(Idt) - 1);
    IdtDescriptor.base  = (uint32_t)&Idt;

    KeLoadIdt();

    /* Update PCR view of IDT */
    Pcr.Idt = &Idt;
}

/* ---------------------------------------------------------------------- */
/* GDT + TSS + PCR setup                                                  */
/* ---------------------------------------------------------------------- */

static void KeSetGdtEntry(int index,
                          uint32_t base,
                          uint32_t limit,
                          uint8_t access,
                          uint8_t gran) {
    Gdt[index].limit_low = (uint16_t)(limit & 0xFFFF);
    Gdt[index].base_low  = (uint16_t)(base & 0xFFFF);
    Gdt[index].base_mid  = (uint8_t)((base >> 16) & 0xFF);
    Gdt[index].access    = access;
    Gdt[index].gran      = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    Gdt[index].base_high = (uint8_t)((base >> 24) & 0xFF);
}

static void KeLoadGdt(void) {
    __asm__ __volatile__("lgdtl (%0)" : : "r" (&GdtDescriptor));
}

static void KeLoadSegments(void) {
    uint16_t data_sel = KGDT_R0_DATA;
    uint16_t pcr_sel  = KGDT_R0_PCR;

    __asm__ __volatile__(
        "movw %0, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        "movw %1, %%ax\n\t"
        "movw %%ax, %%fs\n\t"
        :
        : "r"(data_sel), "r"(pcr_sel)
        : "ax"
    );
    /* CS is assumed to already be KGDT_R0_CODE-compatible from the loader. */
}

static void KeLoadTss(void) {
    uint16_t tss_sel = KGDT_TSS;
    __asm__ __volatile__("ltr %0" : : "r"(tss_sel));
}

static void KeInitTss(void) {
    /* Zero the TSS */
    for (unsigned i = 0; i < sizeof(Tss); i++) {
        ((uint8_t *)&Tss)[i] = 0;
    }

    /* Ring0 stack for privilege transitions (not used yet, but valid) */
    Tss.ss0  = KGDT_R0_DATA;
    Tss.esp0 = (uint32_t)(TssStack + sizeof(TssStack));

    /* Disable I/O bitmap by pointing past TSS limit */
    Tss.iomap_base = sizeof(Tss);

    KeLoadTss();

    /* Update PCR view of TSS */
    Pcr.Tss = &Tss;
}

static void KeInitGdtAndPcr(void) {
    /* Null descriptor */
    KeSetGdtEntry(0, 0, 0, 0x00, 0x00);

    /* Kernel code: base 0, limit 4GB, ring0, executable, readable */
    KeSetGdtEntry(KGDT_R0_CODE >> 3,
                  0x00000000,
                  0x000FFFFF,
                  0x9A,      /* present, ring0, code */
                  0xCF);     /* 4K granularity, 32-bit */

    /* Kernel data: base 0, limit 4GB, ring0, writable */
    KeSetGdtEntry(KGDT_R0_DATA >> 3,
                  0x00000000,
                  0x000FFFFF,
                  0x92,      /* present, ring0, data */
                  0xCF);     /* 4K granularity, 32-bit */

    /* TSS descriptor */
    KeSetGdtEntry(KGDT_TSS >> 3,
                  (uint32_t)&Tss,
                  sizeof(Tss) - 1,
                  0x89,      /* present, ring0, 32-bit TSS (available) */
                  0x00);

    /* PCR descriptor: simple flat data segment for now */
    KeSetGdtEntry(KGDT_R0_PCR >> 3,
                  (uint32_t)&Pcr,
                  sizeof(Pcr) - 1,
                  0x92,      /* present, ring0, data */
                  0x00);

    GdtDescriptor.limit = (uint16_t)(sizeof(Gdt) - 1);
    GdtDescriptor.base  = (uint32_t)&Gdt;

    KeLoadGdt();
    KeLoadSegments();

    /* Initialize minimal PCR contents */
    Pcr.Self = &Pcr;
    Pcr.Gdt  = &Gdt;
    Pcr.Idt  = 0;
    Pcr.Tss  = 0;
}

/* ---------------------------------------------------------------------- */
/* Public entry: full processor bring-up                                  */
/* ---------------------------------------------------------------------- */

void KeInitProcessor(void) {
    KeInitGdtAndPcr();
    KeInitTss();
    KeInitIdt();
}
