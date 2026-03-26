#include <stdint.h>

void vga_write_string(const char *s, uint8_t color);

static void vga_write_hex8(uint32_t value, uint8_t color) {
    char buf[3];
    const char *hex = "0123456789ABCDEF";

    buf[0] = hex[(value >> 4) & 0xF];
    buf[1] = hex[value & 0xF];
    buf[2] = '\0';

    vga_write_string(buf, color);
}

void KePrintCpuInfo(void) {
    uint32_t original_eflags;
    uint32_t modified_eflags;
    uint32_t has_cpuid = 0;

    /* Check CPUID support by toggling EFLAGS.ID (bit 21) */
    __asm__ __volatile__(
        "pushfl\n\t"
        "popl %0\n\t"
        "movl %0, %1\n\t"
        "xorl $0x00200000, %1\n\t"
        "pushl %1\n\t"
        "popfl\n\t"
        "pushfl\n\t"
        "popl %1\n\t"
        : "=r"(original_eflags), "=r"(modified_eflags)
        :
        : "cc"
    );

    if (((original_eflags ^ modified_eflags) & 0x00200000) != 0) {
        has_cpuid = 1;
    }

    if (!has_cpuid) {
        vga_write_string("CPU: no CPUID (likely 386/486)\n", 0x1F);
        return;
    }

    uint32_t eax, ebx, ecx, edx;

    /* Get vendor string (cpuid fn=0) */
    __asm__ __volatile__(
        "movl $0, %%eax\n\t"
        "cpuid\n\t"
        : "=b"(ebx), "=d"(edx), "=c"(ecx)
        :
        : "eax"
    );

    char vendor[13];
    ((uint32_t *)vendor)[0] = ebx;
    ((uint32_t *)vendor)[1] = edx;
    ((uint32_t *)vendor)[2] = ecx;
    vendor[12] = '\0';

    /* Get version info (cpuid fn=1) */
    __asm__ __volatile__(
        "movl $1, %%eax\n\t"
        "cpuid\n\t"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        :
        : 
    );

    uint32_t stepping    =  eax        & 0xF;
    uint32_t model       = (eax >> 4)  & 0xF;
    uint32_t family      = (eax >> 8)  & 0xF;
    uint32_t ext_model   = (eax >> 16) & 0xF;
    uint32_t ext_family  = (eax >> 20) & 0xFF;

    uint32_t final_family = family;
    uint32_t final_model  = model;

    if (family == 0xF) {
        final_family = family + ext_family;
        final_model  = (ext_model << 4) | model;
    }

    vga_write_string("CPU vendor: ", 0x1F);
    vga_write_string(vendor, 0x1F);
    vga_write_string("\n", 0x1F);

    vga_write_string("CPU family: 0x", 0x1F);
    vga_write_hex8(final_family & 0xFF, 0x1F);
    vga_write_string("  model: 0x", 0x1F);
    vga_write_hex8(final_model & 0xFF, 0x1F);
    vga_write_string("  stepping: 0x", 0x1F);
    vga_write_hex8(stepping & 0xF, 0x1F);
    vga_write_string("\n", 0x1F);
}
