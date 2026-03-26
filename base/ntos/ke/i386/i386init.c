#include <stdint.h>

/*
 * Minimal KE/i386 processor bring-up for NaznaOS v0.0.0.5
 *
 * - Installs a private GDT with:
 *     * KGDT_R0_CODE (0x08)
 *     * KGDT_R0_DATA (0x10)
 *     * KGDT_TSS     (0x28)
 *     * KGDT_R0_PCR  (0x30)
 * - Sets up a minimal TSS with a dedicated ring0 stack
 * - Sets up a minimal PCR structure and maps it via FS
 * - Installs a flat IDT with:
 *     * default handler for all vectors
 *     * a real timer interrupt handler on vector 0x20 (IRQ0)
 * - Initializes PIC + PIT and enables interrupts
 */

#define KGDT_R0_CODE 0x08
#define KGDT_R0_DATA 0x10
#define KGDT_TSS     0x28
#define KGDT_R0_PCR  0x30

/* ---------------------------------------------------------------------- */
/* Low-level port I/O                                                     */
/* ---------------------------------------------------------------------- */

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ---------------------------------------------------------------------- */
/* IDT / GDT / TSS / PCR structures                                       */
/* ---------------------------------------------------------------------- */

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

/* ---------------------------------------------------------------------- */
/* Global tables + PCR + TSS                                              */
/* ---------------------------------------------------------------------- */

static struct idt_entry Idt[256];
static struct idt_ptr   IdtDescriptor;

static struct gdt_entry Gdt[7];
static struct gdt_ptr   GdtDescriptor;

static struct tss_entry Tss;
static uint8_t          TssStack[4096];

static struct pcr_minimal Pcr;

/* ---------------------------------------------------------------------- */
/* Enterprise-style tick count (NT-like triple)                           */
/* ---------------------------------------------------------------------- */

typedef struct _KE_TICK_COUNT {
    uint32_t LowPart;     /* +0  */
    uint32_t High1Part;   /* +4  */
    uint32_t High2Part;   /* +8  */
} KE_TICK_COUNT;

volatile KE_TICK_COUNT KeTickCount = {0, 0, 0};
uint32_t KeTimeAdjustment = 1;

/* ---------------------------------------------------------------------- */
/* Default interrupt handler                                              */
/* ---------------------------------------------------------------------- */

__attribute__((noreturn))
static void KeDefaultInterruptHandler(void) {
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

/* ---------------------------------------------------------------------- */
/* Timer interrupt handler (IRQ0, vector 0x20)                            */
/* ---------------------------------------------------------------------- */

/*
 * Naked ISR, full body in asm, no stub.
 * Uses the exact KeTickCount update pattern from legacy clockint.asm:
 *
 *   mov     ecx,eax                 ; copy low tick count
 *   mov     edx,_KeTickCount+4      ; get high tick count
 *   add     ecx,1                   ; increment tick count
 *   adc     edx,0                   ; propagate carry
 *   mov     _KeTickCount+8,edx      ; store high 2 tick count
 *   mov     _KeTickCount+0,ecx      ; store low tick count
 *   mov     _KeTickCount+4,edx      ; store high 1 tick count
 *
 * plus KeTimeAdjustment added into the low part before increment.
 */

__attribute__((naked))
void KeTimerInterrupt(void) {
    __asm__ __volatile__(
        "pusha\n\t"

        /* Load current low tick count into EAX */
        "movl KeTickCount, %eax\n\t"

        /* ECX = low tick count + KeTimeAdjustment */
        "movl %eax, %ecx\n\t"
        "addl KeTimeAdjustment, %ecx\n\t"

        /* EDX = high tick count */
        "movl KeTickCount+4, %edx\n\t"

        /* Increment ECX by 1, propagate carry into EDX */
        "addl $1, %ecx\n\t"
        "adcl $0, %edx\n\t"

        /* Store back:
         *   LowPart   = ECX
         *   High1Part = EDX
         *   High2Part = EDX
         */
        "movl %edx, KeTickCount+8\n\t"
        "movl %ecx, KeTickCount\n\t"
        "movl %edx, KeTickCount+4\n\t"

        /* Send EOI to master PIC (IRQ0) */
        "movb $0x20, %al\n\t"
        "outb %al, $0x20\n\t"

        "popa\n\t"
        "iret\n\t"
    );
}

/* ---------------------------------------------------------------------- */
/* IDT setup                                                              */
/* ---------------------------------------------------------------------- */

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

    /* Default handler for all vectors */
    for (int i = 0; i < 256; i++) {
        KeSetIdtEntry(i, KeDefaultInterruptHandler);
    }

    /* Timer interrupt on vector 0x20 (IRQ0 after PIC remap) */
    KeSetIdtEntry(0x20, KeTimerInterrupt);

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
/* PIC + PIT initialization                                               */
/* ---------------------------------------------------------------------- */

static void KeInitPic(void) {
    /* Remap PIC:
     *  Master: 0x20–0x27
     *  Slave : 0x28–0x2F
     */
    uint8_t master_mask = inb(0x21);
    uint8_t slave_mask  = inb(0xA1);

    /* Start initialization sequence (cascade mode) */
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    /* Set vector offsets */
    outb(0x21, 0x20); /* Master PIC vector offset */
    outb(0xA1, 0x28); /* Slave PIC vector offset */

    /* Tell Master PIC there is a slave at IRQ2 (0000 0100) */
    outb(0x21, 0x04);
    /* Tell Slave PIC its cascade identity (0000 0010) */
    outb(0xA1, 0x02);

    /* Set environment info */
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    /* Restore masks, but unmask IRQ0 on master */
    master_mask &= ~0x01; /* enable IRQ0 */
    outb(0x21, master_mask);
    outb(0xA1, slave_mask);
}

static void KeInitPit(void) {
    /* PIT frequency: 1193182 Hz
     * We choose 100 Hz tick rate => divisor ≈ 11932
     */
    uint16_t divisor = 11932;

    /* Command byte: channel 0, lobyte/hibyte, mode 3 (square wave), binary */
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

/* ---------------------------------------------------------------------- */
/* Public entry: full processor bring-up                                  */
/* ---------------------------------------------------------------------- */

void KeInitProcessor(void) {
    KeInitGdtAndPcr();
    KeInitTss();
    KeInitIdt();
    KeInitPic();
    KeInitPit();

    /* Enable interrupts globally */
    __asm__ __volatile__("sti");
}
