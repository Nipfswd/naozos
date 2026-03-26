#include <stdint.h>

/* Multiboot header in its own section */
__attribute__((section(".multiboot")))
const uint32_t nazna_multiboot_header[] = {
    0x1BADB002,                 /* magic */
    0x00000003,                 /* flags: align + meminfo */
    (uint32_t)-(0x1BADB002 + 0x00000003) /* checksum */
};

void kmain(void); /* defined in base/ntos/init/init.c */

__attribute__((noreturn))
void _start(void) {
    /* GRUB usually gives us a sane stack; for v0.0.0.1 we trust it. */
    kmain();

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
