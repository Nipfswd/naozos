#include <stdint.h>

void vga_init(void);
void vga_clear(uint8_t color);
void vga_write_string(const char *s, uint8_t color);

void kmain(void) {
    vga_init();
    vga_clear(0x1F); /* blue bg, bright white fg */

    vga_write_string("NaznaOS v0.0.0.1\n", 0x1F);
    vga_write_string("base/boot/bldr/i386/initx86.c -> base/ntos/init/init.c\n", 0x1F);
    vga_write_string("Kernel online. Layout respected. Hello, noah.\n", 0x1F);

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
