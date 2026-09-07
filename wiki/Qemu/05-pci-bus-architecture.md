# PCI and Bus Architecture

## Table of Contents
1. [PCI Overview](#pci-overview)
2. [PCI Bus Organization](#pci-bus-organization)
3. [PCI Configuration Space](#pci-configuration-space)
4. [PCI BAR (Base Address Registers)](#pci-bar-base-address-registers)
5. [PCI Interrupts](#pci-interrupts)
6. [DMA and Bus Mastering](#dma-and-bus-mastering)
7. [Hot Plug Support](#hot-plug-support)

## PCI Overview

PCI (Peripheral Component Interconnect) is the standard bus architecture for connecting peripherals in modern computers.

### PCI Hierarchy

```
Root Complex
│
└─ PCI Bus 0 (Primary)
   ├─ Slot 0: Northbridge / MCH
   │  └─ Function 0: Primary PCI-PCI Bridge
   │
   ├─ Slot 1-30: PCI Devices
   │  ├─ Video Card (VGA)
   │  ├─ Network Controller
   │  ├─ SATA Controller
   │  ├─ USB Controller
   │  └─ Audio Device
   │
   └─ Slot 31: ISA Bridge / ICH
      └─ Legacy Devices (Serial, Parallel, etc.)
   
   Secondary PCI Buses (behind bridges):
   ├─ PCI Bus 1 (behind Slot 0)
   │  └─ Devices on secondary bus
   │
   └─ PCI Bus 2 (behind another bridge)
      └─ Devices on secondary bus
```

### PCI Device Addressing

```
Each PCI device identified by:
    [Bus Number : Device Number : Function Number]
    
Example: 01:05:00
    ├─ Bus 1
    ├─ Slot 5 (Device)
    └─ Function 0

Maximum Addressing:
    ├─ 8-bit Bus Number: 0-255 (256 buses)
    ├─ 5-bit Device Number: 0-31 (32 devices per bus)
    └─ 3-bit Function Number: 0-7 (8 functions per device)
    
Total: 256 * 32 * 8 = 65536 possible PCI devices
```

## PCI Bus Organization

### QEMU PCI Bus Implementation

```c
typedef struct PCIBus {
    BusState qbus;
    
    // Bus configuration
    uint8_t number;              // Bus number
    uint8_t subordinate_bus_num; // For bridges
    
    // Devices on this bus
    PCIDevice *devices[32];      // Slot to device mapping
    
    // Bus operations
    const PCIBusOps *bus_ops;
    
    // Interrupt routing
    pci_set_irq_fn set_irq;
    pci_map_irq_fn map_irq;
    
    // Configuration access
    void *irq_opaque;
    
    // Address space
    MemoryRegion *memory;
    MemoryRegion *io;
    
    // Secondary bus (if bridge)
    QLIST_ENTRY(PCIBus) sibling;
} PCIBus;
```

### Device Attachment to PCI Bus

```
1. Device Creation:
   ├─ Allocate PCIDevice structure
   ├─ Initialize properties
   └─ Setup device class

2. Slot Assignment:
   ├─ Find free slot (0-31)
   ├─ Assign device to slot
   └─ Store in bus->devices[slot]

3. BAR Configuration:
   ├─ Allocate I/O address ranges
   ├─ Allocate memory address ranges
   └─ Setup MemoryRegions

4. PCI Configuration Space:
   ├─ Write device ID, vendor ID
   ├─ Setup command register
   └─ Configure BAR addresses

5. Interrupt Assignment:
   ├─ Route IRQ (INTA, INTB, INTC, INTD)
   ├─ Configure interrupt controller
   └─ Enable interrupts

6. Ready for Operation:
   └─> Device can respond to I/O
```

## PCI Configuration Space

PCI Configuration Space is a 256-byte (or 4KB in PCIe) address space for device configuration.

### Header Layout (Type 0 - Endpoint)

```
Offset  Name                        Size    Bits    Description
────────────────────────────────────────────────────────────────
0x00    Vendor ID                   2       [15:0]  Manufacturer ID
0x02    Device ID                   2       [15:0]  Device type ID
0x04    Command                     2       [15:0]  Bus control
0x06    Status                      2       [15:0]  Device status
0x08    Revision ID                 1       [7:0]   Silicon revision
0x09    Prog IF                     1       [7:0]   Programming interface
0x0A    Subclass Code               1       [7:0]   Device subclass
0x0B    Class Code                  1       [7:0]   Device class
0x0C    Cache Line Size             1       [7:0]   Cache line size
0x0D    Latency Timer               1       [7:0]   PCI latency timer
0x0E    Header Type                 1       [7:0]   Header format
0x0F    BIST                        1       [7:0]   Built-in self test
0x10    BAR0                        4       [31:0]  Base address reg 0
0x14    BAR1                        4       [31:0]  Base address reg 1
0x18    BAR2                        4       [31:0]  Base address reg 2
0x1C    BAR3                        4       [31:0]  Base address reg 3
0x20    BAR4                        4       [31:0]  Base address reg 4
0x24    BAR5                        4       [31:0]  Base address reg 5
0x28    Cardbus CIS Pointer         4       [31:0]  Card data pointer
0x2C    Subsystem Vendor ID         2       [15:0]  Sub-vendor ID
0x2E    Subsystem Device ID         2       [15:0]  Sub-device ID
0x30    Expansion ROM Base Address  4       [31:0]  ROM address
0x34    Capabilities Pointer        1       [7:0]   Cap pointer (x in PCI config space)
0x35    Reserved                    3               Reserved
0x38    Reserved                    4               Reserved
0x3C    Interrupt Line              1       [7:0]   Interrupt line (IRQ)
0x3D    Interrupt Pin               1       [7:0]   Interrupt pin (INTA-INTD)
0x3E    Min Grant                   1       [7:0]   Burst period length
0x3F    Max Latency                 1       [7:0]   Max latency
```

### Configuration Space Access Mechanisms

```
Mechanism 1 (Standard - Type 0 & 1):
    - Two I/O ports: 0xCF8 (address), 0xCFC (data)
    - Address format: [Enable][Reserved][Bus][Device][Function][Offset]
    
Mechanism 2 (Obsolete - Type 1 only):
    - I/O ports 0xC000-0xCFFF
    - Direct forwarding to device

QEMU Implementation:
    - Emulates Mechanism 1
    - Software accesses 0xCF8/0xCFC
    - QEMU intercepts and routes to device
```

## PCI BAR (Base Address Registers)

BARs define where the device's I/O and memory regions appear in the host address space.

### BAR Format

```
I/O BAR:
    [31:2]  Base Address       (I/O address in host)
    [1]     Reserved (0)
    [0]     Type Indicator (1 = I/O)

Memory BAR:
    [31:4]  Base Address       (Memory address in host)
    [3]     Prefetchable
    [2:1]   Type (0 = 32-bit, 2 = 64-bit)
    [0]     Type Indicator (0 = Memory)
```

### BAR Size Detection (Discovery)

```
Guest (BIOS) BAR Discovery:

1. Write all 1s to BAR register:
   write(0x10, 0xFFFFFFFF)
   
2. Read back register:
   val = read(0x10)
   
3. Calculate size:
   size = ~(val & MASK) + 1
   
   Example:
   - Write 0xFFFFFFFF to 32-bit BAR
   - Read back 0xFFFFF000 (512 BAR bits are RW)
   - Size = ~0xFFFFF000 + 1 = 0x1000 (4KB)

4. Assign BAR address:
   write(0x10, allocated_base_address)
   └─> Device now responds to I/O at allocated address
```

### QEMU BAR Management

```
1. Device Definition:
   const MemoryRegionOps device_mmio_ops = {
       .read = device_mmio_read,
       .write = device_mmio_write,
   };

2. BAR Registration:
   pci_register_bar(pci_dev, 0, 
       PCI_BASE_ADDRESS_SPACE_MEMORY,
       &dev->mmio_region);

3. QEMU tracks BAR size:
   ├─ When guest writes BAR address
   ├─ MemoryRegion size already known
   └─> Device responds within allocated region

4. Guest I/O Routing:
   Guest Access → QEMU → MemoryRegion Handler → Device
```

## PCI Interrupts

### Interrupt Routing

```
PCI Device Interrupts:

Level-Triggered (Standard):
    ├─ Device asserts interrupt line
    ├─ Holds until cleared
    └─ Can be shared between devices

Edge-Triggered (Modern - MSI/MSIX):
    ├─ Device sends interrupt message
    ├─ Single transaction, no holding
    └─ Cannot be shared

QEMU Routing:
    PCI Device
        ├─ INTA/INTB/INTC/INTD
        └─> PCI-to-ISA Bridge
            └─> Interrupt Controller (PIC/APIC)
                └─> CPU Interrupt
```

### Interrupt Pin Mapping

```
Each PCI device has 4 possible interrupt pins:
    ├─ INTA (pin 1)
    ├─ INTB (pin 2)
    ├─ INTC (pin 3)
    └─ INTD (pin 4)

Multi-function devices:
    Function 0: INTA
    Function 1: INTB
    Function 2: INTC
    Function 3: INTD
    Function 4: INTA
    ... (repeats)
```

### MSI (Message Signaled Interrupts)

```
Traditional Interrupts:
    Device ─ IRQ Signal ─> Interrupt Controller

MSI:
    Device ─ Memory Write ─> Host Memory Address
                           └─> Trigger interrupt

Advantages:
    ├─ No shared interrupt lines
    ├─ Lower latency
    ├─ No spurious interrupts
    └─ Supports more interrupts

MSI-X (Extended):
    ├─ Supports up to 2048 interrupts per device
    ├─ Table-based configuration
    └─ More flexible routing
```

## DMA and Bus Mastering

### Bus Mastering Concept

```
Traditional I/O:
    CPU ─ I/O Port/Memory ─> Device
    (CPU controls data transfer)

Bus Mastering (DMA):
    Device ──────DMA────────> RAM
    ├─ Device reads command from memory
    ├─ Performs data transfer
    ├─ Writes back status/results
    └─> Signals completion via interrupt

Benefits:
    ├─ CPU not involved in data transfer
    ├─ Higher throughput
    └─ Lower latency
```

### DMA Address Translation

```
Device View:
    Device wants to read buffer at "address X"
    
Physical Address Space:
    X ──> Guest Physical Address (GPA)
    
Guest Page Tables:
    GPA -> Guest Virtual Address (GVA)
    
Host Address Space:
    GPA -> Host Virtual Address (HVA)
    
Actual Memory:
    HVA -> Host Physical Address (HPA)

With IOMMU:
    DMA Request ─> IOMMU Translation ─> Host Physical
    (Allows remapping and restriction)
```

## Hot Plug Support

### PCI Hot Plug Architecture

```
Guest OS:
    ├─ ACPI Notifications
    ├─ PCI Slot Register
    └─ Hot Plug Handler

QEMU Hotplug Layer:
    ├─ Device Add/Remove
    ├─ Slot Management
    └─ Event Notification

PCI Bus:
    ├─ Device Attachment/Detachment
    └─ Configuration Space Updates
```

### Hotplug Device Add Sequence

```
1. Monitor Command:
   device_add qemu-xhci,id=xhci

2. QEMU Instantiation:
   ├─ Create device object
   ├─ Apply properties
   └─> Realize device

3. Bus Attachment:
   ├─ Find free PCI slot
   ├─ Assign slot/function
   └─> Configure PCI header

4. Guest Notification:
   ├─ Generate ACPI hot-plug event
   └─> BIOS/OS detects new device

5. Guest Driver:
   ├─ Query device via PCI config space
   ├─ Load appropriate driver
   └─> Device becomes operational
```

---

*See related documents: [Device Model Architecture](04-device-model.md), [I/O and Interrupt Handling](09-io-interrupts.md)*
