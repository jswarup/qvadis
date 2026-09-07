# I/O and Interrupt Handling

## Table of Contents
1. [Interrupt Architecture](#interrupt-architecture)
2. [PIC (Programmable Interrupt Controller)](#pic-programmable-interrupt-controller)
3. [APIC (Advanced Programmable Interrupt Controller)](#apic-advanced-programmable-interrupt-controller)
4. [GIC (Generic Interrupt Controller - ARM)](#gic-generic-interrupt-controller--arm)
5. [IRQ Routing](#irq-routing)
6. [I/O Port Emulation](#io-port-emulation)
7. [Memory-Mapped I/O](#memory-mapped-io)

## Interrupt Architecture

### Interrupt Hierarchy

```
Device Interrupt Request
        │
        ▼
┌──────────────────────────────┐
│  Interrupt Controller        │
│  (PIC/APIC/GIC)             │
│  ├─ Priority resolver       │
│  ├─ Edge/Level detection    │
│  └─ CPU delivery logic      │
└────────────┬─────────────────┘
             │
             ▼
    ┌─────────────────┐
    │  CPU Exception  │
    │  (int xx)       │
    └────────┬────────┘
             │
             ▼
    ┌──────────────────┐
    │  Exception Handler│
    │  (IDT vector)    │
    └────────┬─────────┘
             │
             ▼
    ┌────────────────────────┐
    │  Interrupt Service     │
    │  Routine (ISR)         │
    │  (Guest driver)        │
    └────────────────────────┘
```

### Interrupt Request (IRQ) Numbering

```
x86 PC Architecture:

Master PIC (IRQ 0-7):
    ├─ IRQ 0: System timer (PIT)
    ├─ IRQ 1: Keyboard
    ├─ IRQ 2: Cascade to slave PIC
    ├─ IRQ 3: Serial port 2
    ├─ IRQ 4: Serial port 1
    ├─ IRQ 5: Parallel port / Sound card
    ├─ IRQ 6: Floppy disk
    └─ IRQ 7: Parallel port / Printer

Slave PIC (IRQ 8-15):
    ├─ IRQ 8: Real-time clock
    ├─ IRQ 9: Video card / Redirected to IRQ 2
    ├─ IRQ 10: Reserved / PCI
    ├─ IRQ 11: Reserved / PCI
    ├─ IRQ 12: PS/2 mouse
    ├─ IRQ 13: Coprocessor
    ├─ IRQ 14: Primary IDE
    └─ IRQ 15: Secondary IDE

PCI Interrupts:
    ├─ INTA, INTB, INTC, INTD
    └─ Mapped to ISA IRQs by bridge logic
```

## PIC (Programmable Interrupt Controller)

### PIC Architecture

```
Master PIC                          Slave PIC
┌──────────────────┐            ┌──────────────────┐
│ IRQ 0-7 inputs   │            │ IRQ 8-15 inputs  │
│  IRR (In-Service)│            │  IRR (In-Service)│
│  IMR (Mask)      │            │  IMR (Mask)      │
│  ISR (Request)   │            │  ISR (Request)   │
└────────┬─────────┘            └────────┬─────────┘
         │                               │
         │    ┌────────────────────┐    │
         └───►│ Priority Resolver  │◄───┘
              │ (Priority arbiter) │
              └────────┬───────────┘
                       │
                       ▼
              ┌──────────────────┐
              │  IRQ output (to  │
              │  CPU INT pin)    │
              └──────────────────┘
```

### Interrupt Request Processing

```
1. Device asserts IRQ line (Level-triggered):
   ├─ Hold IRQ line HIGH
   └─ Keep asserted until cleared

2. PIC detects IRQ:
   ├─ Set bit in IRR (Interrupt Request Register)
   ├─ Check if masked (IMR - Interrupt Mask Register)
   └─ Route to CPU if enabled

3. CPU acknowledges interrupt:
   ├─ INTA signal from CPU
   ├─ PIC responds with interrupt vector
   ├─ Set bit in ISR (In-Service Register)
   └─> CPU jumps to handler

4. CPU processes interrupt:
   ├─ Save state (CPU pushes flags, CS:EIP)
   ├─ Jump to handler address (from IDT)
   └─ Handler does work

5. CPU sends EOI (End Of Interrupt):
   ├─ Write EOI command to PIC
   ├─ Clear bit in ISR
   └─> PIC ready for next interrupt

6. CPU resumes (IRET instruction):
   ├─ Restore saved context
   └─> Return to interrupted code
```

### PIC Registers (I/O Ports)

```
Master PIC:
    0x20: Command/Status Register
    0x21: Interrupt Mask Register (IMR)

Slave PIC:
    0xA0: Command/Status Register
    0xA1: Interrupt Mask Register (IMR)

Commands:
    Initialization Command Word (ICW1-4)
    ├─ ICW1 (0x11): Edge triggered, cascade mode
    ├─ ICW2: Base interrupt vector
    ├─ ICW3: Cascade configuration
    └─ ICW4: Buffer/automatic EOI mode

    Operation Command Words (OCW1-3)
    ├─ OCW1: Set/clear interrupt masks
    ├─ OCW2: Rotate priority, EOI
    └─ OCW3: Read registers (IRR/ISR)
```

## APIC (Advanced Programmable Interrupt Controller)

### APIC Overview

```
APIC Components:

┌─────────────────────────────────┐
│  APIC Bus (per CPU)             │
│                                 │
│  ┌──────────────────────────┐  │
│  │  Local APIC (per CPU)    │  │
│  │  ├─ Timer                │  │
│  │  ├─ Thermal Monitor      │  │
│  │  ├─ Performance Counter  │  │
│  │  ├─ LINT0 (NMI)          │  │
│  │  ├─ LINT1 (INTR)         │  │
│  │  └─ Error handler        │  │
│  └──────────────────────────┘  │
│                                 │
│  ┌──────────────────────────┐  │
│  │  IO APIC (System bus)    │  │
│  │  ├─ Up to 24 input pins  │  │
│  │  ├─ Priority logic       │  │
│  │  ├─ Trigger mode         │  │
│  │  └─ Destination routing  │  │
│  └──────────────────────────┘  │
└─────────────────────────────────┘
       ▲
       │
 External Interrupts
```

### Local APIC

```
Local APIC Registers (Memory-Mapped):

0xFEE00000 + Offset     Register                Size
─────────────────────────────────────────────────────
0x020                   Local APIC ID           32-bit
0x030                   Local APIC Version      32-bit
0x080                   Task Priority Register  32-bit
0x0A0                   Arbitration Priority    32-bit
0x0B0                   Processor Priority      32-bit
0x0D0                   EOI Register            32-bit
0x0E0                   Remote Read Register    32-bit
0x0F0                   Logical Destination     32-bit
0x100-0x170             In-Service Register     8 x 32-bit
0x180-0x1F0             Trigger Mode Register   8 x 32-bit
0x200-0x270             Interrupt Request Reg   8 x 32-bit
0x280                   Error Status Register   32-bit
0x300                   Interrupt Command Reg   64-bit (Lo/Hi)
0x320                   LVT Timer               32-bit
0x330                   LVT Thermal Monitor     32-bit
0x340                   LVT Performance Count   32-bit
0x350                   LVT LINT0               32-bit
0x360                   LVT LINT1               32-bit
0x370                   LVT Error               32-bit
0x380                   Timer Initial Count     32-bit
0x390                   Timer Current Count     32-bit
0x3E0                   Timer Divide Config     32-bit
```

### Interrupt Delivery Modes

```
Mode Type       Description
──────────────────────────────────────────────────────
Fixed           Send to destination(s)
Lowest Priority Send to CPU with lowest priority
SMI             System Management Interrupt (x86)
NMI             Non-Maskable Interrupt
INIT            Processor initialization
Start-Up        Startup after reset
ExtINT          External interrupt (compatible mode)
SIPI            Startup IPI
```

## GIC (Generic Interrupt Controller - ARM)

### GIC Architecture

```
GIC Distributor (centralizes interrupt control)
    ├─ Handles up to 1020 interrupt lines
    ├─ Selects destination CPUs
    ├─ Sets priority levels
    └─ Enables/disables interrupts

CPU Interface (per-CPU):
    ├─ Acknowledges interrupts
    ├─ Provides interrupt priority filtering
    ├─ Sends EOI (end of interrupt)
    └─ CPU-specific status

Virtual Interface (for guests/hypervisors):
    ├─ Trap-based interrupt delivery
    ├─ Virtual interrupt injection
    ├─ Maintains guest interrupt state
    └─ List register for pending interrupts
```

### GIC Interrupt Types

```
SGI (Software Generated Interrupts):
    ├─ IRQ 0-15
    ├─ Generated by software
    └─ Used for inter-processor communication

PPI (Private Peripheral Interrupts):
    ├─ IRQ 16-31
    ├─ Per-CPU private interrupts
    └─ Examples: CPU timer, watchdog

SPI (Shared Peripheral Interrupts):
    ├─ IRQ 32-1019
    ├─ Shared among all CPUs
    └─ Examples: Devices on shared bus
```

## IRQ Routing

### IRQ Routing in QEMU

```
Device Interrupt Request
        │
        ▼
┌──────────────────────────┐
│ Device Model             │
│ (Device emulation code)  │
│ qemu_irq_raise/lower()   │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ IRQ Handler              │
│ (Bus handler)            │
│ pci_set_irq()            │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ Interrupt Controller     │
│ (PIC/APIC handler)       │
│ pic_update_irq()         │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ CPU Exception            │
│ (CPU emulation)          │
│ cpu_interrupt()          │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ Guest Interrupt Handler  │
│ (Guest OS/Driver)        │
└──────────────────────────┘
```

### PCI IRQ Routing Table

```
QEMU PCI Routing (x86 PC):

PCI Slot    INTA    INTB    INTC    INTD
─────────────────────────────────────────
Slot 1      IRQ 10  IRQ 10  IRQ 10  IRQ 10
Slot 2      IRQ 11  IRQ 5   IRQ 11  IRQ 5
Slot 3      IRQ 5   IRQ 11  IRQ 5   IRQ 11
...
Slot 31     IRQ 9   IRQ 9   IRQ 9   IRQ 9

Notes:
    ├─ Each slot/function can use multiple INTA-INTD
    ├─ Same interrupt can be shared (PCI IRQ sharing)
    ├─ Routing defined in BIOS PIRQ table
    └─ Can be changed via software (if supported)
```

## I/O Port Emulation

### Port-Based I/O

```
Guest I/O Instruction:
    OUT 0x21, AL        ; Write to PIC IMR
    IN  AL, 0x60        ; Read from keyboard

QEMU Interception:

1. Instruction Decode:
   ├─ TCG: Detect IN/OUT instruction
   ├─ KVM: Exit on port access
   └─> Determine port number and direction

2. Lookup Handler:
   ├─ Search I/O port map
   ├─ Find device handling port
   └─> Call read/write handler

3. Execute Handler:
   ├─ Device model handles I/O
   ├─ Read from state/memory
   ├─ Write to state/memory
   └─> Generate interrupt if needed

4. Return Result:
   ├─ Return value (for IN)
   ├─ Update guest registers
   └─> Resume execution
```

### Port Registration

```c
// Device registers for I/O port handling:

MemoryRegionOps device_io_ops = {
    .read = device_io_read,
    .write = device_io_write,
};

MemoryRegion io_region;
memory_region_init_io(&io_region, NULL, &device_io_ops,
                      device_state, "device-io", 8);  // 8 bytes

isa_register_portio_list(dev, &dev->portio_list,
                         0x60, device_io_list,
                         device, "device-io");
```

## Memory-Mapped I/O

### MMIO Architecture

```
Guest Code:
    mov eax, [0xFEE00080]    ; Read APIC TPR register
    mov [0xFEE00380], ecx    ; Write APIC timer

QEMU Handling:

1. Guest Virtual Address:
   0xFEE00080 (in guest page table)

2. Translation:
   Guest Virtual → Guest Physical (0xFEE00080)

3. Memory Region Lookup:
   ├─ Find MemoryRegion covering 0xFEE00080
   ├─ Identify handler (APIC controller)
   └─> Call handler

4. Handler Execution:
   ├─ Read/write APIC state
   ├─ Update internal registers
   └─> Generate effects (interrupt, etc.)

5. Return Data:
   ├─ Return value for read
   └─> Resume execution
```

### MMIO Region Examples

```
x86 PC Memory Map:

0x00000000-0x0009FFFF: Conventional RAM (640KB)
0x000A0000-0x000BFFFF: Video RAM (VGA)
0x000C0000-0x000FFFFF: BIOS ROM
0x00100000-0xFFFDFFFF: Extended RAM
0xFEE00000-0xFEE00FFF: Local APIC
0xFEC00000-0xFEC00FFF: I/O APIC
0xFED00000-0xFED003FF: HPET (High Precision Timer)
0xFED40000-0xFED44FFF: TPM (Trusted Platform Module)
0xFED80000-0xFED8FFFF: MCH (Memory Controller Hub)
0xFEE00000-0xFFFFFFFF: BIOS ROM extension
```

---

*See related documents: [PCI and Bus Architecture](05-pci-bus-architecture.md), [Device Model Architecture](04-device-model.md)*
