#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEM    ((uint16_t*)0xB8000)

static uint16_t *vga_buffer = VGA_MEM;
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

static uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void vga_put_char(char c, uint8_t color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        const uint32_t idx = (uint32_t)cursor_y * VGA_WIDTH + cursor_x;
        vga_buffer[idx] = vga_entry(c, color);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    if (cursor_y >= VGA_HEIGHT) {
        cursor_y = 0; /* wrap; no scrolling yet */
    }
}

static void vga_write_string(const char *s, uint8_t color) {
    while (*s) {
        vga_put_char(*s++, color);
    }
}

static void vga_clear(uint8_t color) {
    for (uint32_t y = 0; y < VGA_HEIGHT; y++) {
        for (uint32_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
}

static void vga_init(void) {
    /* reserved for future hardware init */
}
