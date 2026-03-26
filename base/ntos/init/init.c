#include <stdint.h>

void vga_init(void);
void vga_clear(uint8_t color);
void vga_write_string(const char *s, uint8_t color);

/* KE/i386 processor + GDT/TSS/PCR + IDT init */
void KeInitProcessor(void);

/* KE/i386 CPU info */
void KePrintCpuInfo(void);

void kmain(void) {
    vga_init();
    vga_clear(0x1F); /* blue bg, bright white fg */

    vga_write_string("NaznaOS v0.0.0.4\n", 0x1F);
    vga_write_string("base/boot/bldr/i386/initx86.c -> base/ntos/init/init.c\n", 0x1F);
    vga_write_string("Kernel online. Layout respected. Hello, noah.\n", 0x1F);

    vga_write_string("\nBringing up KE/i386 CPU + GDT/TSS/PCR + IDT...\n", 0x1F);
    KeInitProcessor();
    vga_write_string("KE/i386 processor environment initialized.\n", 0x1F);

    vga_write_string("\nQuerying CPU via KE/i386 CPUID logic...\n", 0x1F);
    KePrintCpuInfo();

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
