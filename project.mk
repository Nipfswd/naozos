CC      := i686-elf-gcc
LD      := i686-elf-ld

CFLAGS  := -std=gnu99 -ffreestanding -O2 -Wall -Wextra -m32 \
           -fno-stack-protector -fno-pic -fno-builtin
LDFLAGS := -T linker.ld -nostdlib

TARGET      := naznaos
BUILD_DIR   := build
ISO_DIR     := $(BUILD_DIR)/iso
BOOT_DIR    := $(ISO_DIR)/boot
GRUB_DIR    := $(BOOT_DIR)/grub
KERNEL_BIN  := $(BUILD_DIR)/kernel.bin
