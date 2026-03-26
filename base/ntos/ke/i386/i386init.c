#include <stdint.h>

/*
 * Minimal KE/i386 interrupt skeleton for NaznaOS v0.0.0.2
 * - Sets up a flat IDT with a single default handler
 * - Does NOT enable interrupts yet (no sti here)
 * - Uses GRUB's existing GDT and code segment selector (0x08)
 */

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

static struct idt_entry Idt[256];
static struct idt_ptr   IdtDescriptor;

/* Default handler: just halt forever for now */
__attribute__((noreturn))
static void KeDefaultInterruptHandler(void) {
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

static void KeSetIdtEntry(int vec, void (*handler)(void)) {
    uint32_t base = (uint32_t)handler;

    Idt[vec].base_low  = (uint16_t)(base & 0xFFFF);
    Idt[vec].sel       = 0x08;      /* kernel code segment */
    Idt[vec].always0   = 0;
    Idt[vec].flags     = 0x8E;      /* present, ring0, 32-bit interrupt gate */
    Idt[vec].base_high = (uint16_t)((base >> 16) & 0xFFFF);
}

static void KeLoadIdt(void) {
    __asm__ __volatile__("lidtl (%0)" : : "r" (&IdtDescriptor));
}

/*
 * Public entry: called from base/ntos/init/init.c
 */
void KeInitInterrupts(void) {
    /* Zero the IDT */
    for (int i = 0; i < 256; i++) {
        Idt[i].base_low  = 0;
        Idt[i].sel       = 0;
        Idt[i].always0   = 0;
        Idt[i].flags     = 0;
        Idt[i].base_high = 0;
    }

    /* For now, point all vectors at a single default handler */
    for (int i = 0; i < 256; i++) {
        KeSetIdtEntry(i, KeDefaultInterruptHandler);
    }

    IdtDescriptor.limit = (uint16_t)(sizeof(Idt) - 1);
    IdtDescriptor.base  = (uint32_t)&Idt;

    KeLoadIdt();
}
