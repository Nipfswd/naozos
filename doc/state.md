# State for copilot

# ⭐ **NaznaOS — Current State (v0.0.4)**  
*A clean, accurate summary of the OS you have built so far.*

---

## 🧩 **Boot Path**
### **1. GRUB → initx86.c**
- GRUB loads your kernel at 1MB.
- Control enters:
  ```
  base/boot/bldr/i386/initx86.c
  ```
- This file switches to C and calls the kernel entrypoint.

### **2. init.c**
Located at:
```
base/ntos/init/init.c
```
It does:
- VGA init + clear  
- Prints the NaznaOS banner  
- Calls:
  - `KeInitProcessor()` → sets up CPU environment  
  - `KePrintCpuInfo()` → CPUID logic  
- Enters an infinite `hlt` loop

---

## 🧠 **CPU Bring‑Up (v0.0.4)**
### **File:**  
```
base/ntos/ke/i386/i386init.c
```

### **What it sets up:**
- **GDT**  
  - Null descriptor  
  - Kernel code (0x08)  
  - Kernel data (0x10)  
  - TSS (0x28)  
  - PCR segment (0x30)

- **TSS**  
  - Minimal structure  
  - Valid `esp0` and `ss0`  
  - Loaded via `ltr`

- **PCR (Processor Control Region)**  
  - Minimal C struct  
  - Contains pointers to:
    - Self  
    - GDT  
    - IDT  
    - TSS  
  - Loaded into **FS** selector (0x30)

- **IDT**  
  - 256 entries  
  - All mapped to a default stub handler  
  - Loaded via `lidt`

This is a *real* CPU environment — not GRUB’s leftovers.

---

## 🧬 **CPUID Logic**
### **File:**  
```
base/ntos/ke/i386/kex86.c
```

### **What it does:**
- Detects CPUID support by toggling EFLAGS.ID  
- Reads vendor string  
- Reads family/model/stepping  
- Prints them via VGA

This is why your QEMU window shows:

```
CPU vendor: GenuineIntel
CPU family: 0x06 model: 0x06 stepping: 0x03
```

---

## 🎨 **VGA Output**
### **File:**  
```
drivers/vga.c
```
(Not shown, but we know it exists because the kernel links it.)

Used by:
- `vga_init()`
- `vga_clear()`
- `vga_write_string()`

---

## 📦 **SDK Includes**
### **Folder:**  
```
public/sdk/inc/
```

Contains:
- `callconv.inc`  
- `ks386.inc`  
- (and likely other public headers)

These are used by all KE/i386 assembly files.

---

## 🧱 **Old OS Artifacts You Provided**
These are **not** part of NaznaOS yet, but you’ve shown them for reference:

- `cpu.asm`  
- `cpu.inc`  
- `i386pcr.asm`  
- `clockint.asm`  
- `timindex.asm`

They give us the blueprint for future subsystems:
- PCR layout  
- PRCB layout  
- trap frame offsets  
- timer logic  
- profiling  
- DPC scheduling  
- etc.

But **NaznaOS is not using them yet** — we’re building clean, minimal versions.

---

# ⭐ **NaznaOS Right Now (One Sentence)**  
A clean, booting kernel with VGA output, a real GDT/TSS/PCR, a real IDT, and working CPUID logic — ready for interrupts, timers, and scheduling.