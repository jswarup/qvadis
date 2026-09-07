# Machine and Hypervisor Integration

## Table of Contents
1. [Machine Types](#machine-types)
2. [Firmware and Boot](#firmware-and-boot)
3. [KVM Integration](#kvm-integration)
4. [Hypervisor Backends](#hypervisor-backends)
5. [Machine-Specific Features](#machine-specific-features)
6. [Device Tree Architecture](#device-tree-architecture)

## Machine Types

A machine type defines the complete hardware configuration of a virtual machine.

### Machine Type Components

```
Machine Type Definition:

├─ CPU(s)
│  ├─ Architecture (x86, ARM, MIPS, etc.)
│  ├─ Model (Nehalem, Cortex-A57, etc.)
│  └─ Count (1 to N cores)
│
├─ Memory Layout
│  ├─ Base address
│  ├─ Maximum size
│  └─ Hotplug support
│
├─ System Devices
│  ├─ Motherboard chipset
│  ├─ Interrupt controller (PIC, APIC, etc.)
│  ├─ Timer/Clock devices
│  ├─ Real-time clock (RTC)
│  └─ System controller
│
├─ Bus Architecture
│  ├─ PCI host bridge
│  ├─ ISA/Legacy bus
│  ├─ USB host controllers
│  └─ Other buses (I2C, SPI)
│
├─ Peripherals
│  ├─ Storage controllers (AHCI, SCSI)
│  ├─ Network interfaces
│  ├─ Audio devices
│  ├─ Graphics cards
│  └─ Other devices
│
└─ Firmware
   ├─ BIOS ROM location and size
   ├─ UEFI firmware (if supported)
   └─ Device ROM locations (Option ROMs)
```

### x86 PC Machine Type

```
x86 PC Architecture:

RAM (Guest Physical Address Space):
┌────────────────────────────────┐
│ 0xFFFFFFFF (4GB)               │
├────────────────────────────────┤
│  PCI Address Space             │
│  (PCI devices: 0xC000_0000 to  │
│   0xFFDF_0000)                 │
├────────────────────────────────┤
│  Device ROM (Option ROM)       │
│  (0xC000_0000 to 0xF000_0000)  │
├────────────────────────────────┤
│  Free                          │
├────────────────────────────────┤
│  Video RAM (if VGA)            │
│  (0xA000_0000 to 0xB000_0000)  │
├────────────────────────────────┤
│  Free                          │
├────────────────────────────────┤
│  Extended Memory               │
│  (0x100_0000 onwards)          │
├────────────────────────────────┤
│  High Memory (1MB)             │
│  (0xF000_0 to 0x100_000)       │
├────────────────────────────────┤
│  ISA Memory/Devices            │
│  (0x10_000 to 0xF_000)         │
├────────────────────────────────┤
│  Conventional Memory (640KB)   │
│  (0x0 to 0xA_000)              │
└────────────────────────────────┘

Legacy Memory Map:
├─ 0x0 - 0xFFF: Exception vectors, BIOS data
├─ 0x1000 - 0x9FFFF: Conventional RAM (640KB)
├─ 0xA0000 - 0xBFFFF: Video memory (VGA text/graphics)
├─ 0xC0000 - 0xFFFFF: BIOS ROM + Option ROMs
└─ 0x100000+: Extended memory (1MB+)

Key Devices:
├─ CPU (x86-64)
├─ i440FX Chipset (Northbridge)
├─ PIIX3 Chipset (Southbridge, ISA Bridge)
├─ PIC (8259A) - Interrupt controller
├─ APIC (Advanced) - Interrupt controller
├─ PCI Bus (behind i440FX)
├─ ISA Bus (behind PIIX3)
├─ CMOS/RTC (real-time clock)
├─ PS/2 Controller (keyboard/mouse)
├─ UART (serial port)
├─ Timer (PIT - Programmable Interval Timer)
├─ AHCI (SATA controller)
├─ E1000/Virtio-Net (network)
└─ VGA (graphics)
```

### ARM Virt Machine Type

```
ARM Virtual Machine (Cortex-A57):

┌────────────────────────────────┐
│  ARM Virt Board Architecture    │
├────────────────────────────────┤
│  Memory Layout:                │
│  0x0 - 0x0800_0000: RAM        │
│  (2GB default, configurable)   │
│                                │
│  0x0800_0000 - 0x1000_0000:    │
│  Reserved for future RAM       │
│                                │
│  0x0900_0000 - 0x0A00_0000:    │
│  PCI Address Space             │
│                                │
│  0x0A00_0000 - 0x0B00_0000:    │
│  PCI I/O Space                 │
│                                │
│  0x0E00_0000 - 0x1000_0000:    │
│  System devices                │
│  ├─ GIC (Interrupt controller) │
│  ├─ UART                       │
│  ├─ RTC                        │
│  ├─ Timer                      │
│  ├─ Watchdog                   │
│  └─ PCIE Controller            │
└────────────────────────────────┘

Key Devices:
├─ ARM CPU(s) (Cortex-A57/A72/etc.)
├─ GIC (Generic Interrupt Controller)
│  ├─ Distributor
│  ├─ CPU Interface (per CPU)
│  └─ Virtual interface (for guests)
├─ PCI Express Controller
│  ├─ PCIE Bus 0
│  └─ Device attachment points
├─ System Timer
├─ Watchdog Timer
├─ RTC (Real-Time Clock)
├─ UART (Serial console)
└─ SMC (System Monitor Call - for poweroff/reboot)

Boot Process:
1. CPU starts at address 0
2. Loads kernel from DTB/device tree
3. BIOS (firmware) sets up memory
4. Kernel initializes devices
5. Userspace starts
```

### RISC-V Virt Machine Type

```
RISC-V Virtual Machine:

Features:
├─ Modular CPU architecture
├─ Simple device set
├─ Device tree based configuration
└─ Memory-mapped I/O

Device Layout:
├─ UART (Serial console)
├─ SPI Flash (firmware storage)
├─ PLIC (Platform Level Interrupt Controller)
├─ CLINT (Core Local Interruptor)
├─ Test Control (for testing)
└─ PCI Express (optional)

Advantages:
├─ Minimal hardware dependencies
├─ Extensible architecture
└─ Easy to add custom devices
```

## Firmware and Boot

### BIOS (Basic Input/Output System)

```
BIOS Roles:

1. Power-On Self Test (POST)
   ├─ Verify hardware
   ├─ Detect RAM
   ├─ Initialize basic devices
   └─> Report status to guest

2. Device Enumeration
   ├─ PCI device discovery
   ├─ USB device discovery
   ├─ Disk detection
   └─ Network detection

3. Bootloader
   ├─ Select boot device
   ├─ Load bootloader (MBR, PBR)
   ├─ Execute bootloader code
   └─> Pass control to OS

BIOS Location (x86):
    ├─ Mapped at 0xFFFF_0000 (64KB)
    ├─ Or configurable via command-line
    └─ Typically SEABIOS or OpenBIOS

BIOS in QEMU:
    ├─ Pre-compiled firmware image
    ├─ Located in share/bios-*.bin
    ├─ Mapped into guest address space
    └─ Executed before OS kernel
```

### UEFI (Unified Extensible Firmware Interface)

```
UEFI Advantages:
    ├─ GPT (GUID Partition Table) support
    ├─ Large disks (>2TB)
    ├─ Modular architecture
    ├─ Network boot capabilities
    ├─ Secure boot support
    └─ Better hardware detection

UEFI in QEMU:
    ├─ OVMF (Open Virtual Machine Firmware)
    ├─ UEFI firmware for x86/x64
    ├─ Supports Secure Boot
    ├─ Variables stored in NVRAM
    └─ Located at: share/OVMF_CODE.fd, OVMF_VARS.fd

Usage:
    -drive file=OVMF_CODE.fd,format=raw,if=pflash,unit=0,readonly=on
    -drive file=OVMF_VARS.fd,format=raw,if=pflash,unit=1
```

### Device Tree (ARM/RISC-V)

```
Device Tree Structure:

├─ Root node (/)
│
├─ /cpus
│  ├─ /cpu@0
│  │  ├─ compatible = "arm,cortex-a57"
│  │  ├─ reg = <0>
│  │  └─ clock-frequency = <1000000000>
│  └─ /cpu@1, /cpu@2, ...
│
├─ /memory@80000000
│  ├─ device_type = "memory"
│  ├─ reg = <0x80000000 0x40000000>  # 1GB at 0x80000000
│  └─ device_type = "memory"
│
├─ /timer
│  ├─ compatible = "arm,armv8-timer"
│  └─ interrupts = <...>
│
├─ /gic@8000000
│  ├─ compatible = "arm,gic-v2"
│  ├─ reg = <0x8000000 0x1000>, <0x8010000 0x1000>
│  ├─ interrupts = <...>
│  └─ #interrupt-cells = <3>
│
├─ /uart@9000000
│  ├─ compatible = "ns16550a"
│  ├─ reg = <0x9000000 0x100>
│  ├─ clock-frequency = <1843200>
│  └─ interrupts = <33>
│
└─ /pcie@10000000
   ├─ compatible = "pci-host-cam-generic"
   └─ ...

Device Tree Blob (DTB):
    ├─ Compiled binary format
    ├─ Passed to kernel
    ├─ Kernel parses and enumerates devices
    └─ No need for hardcoded device addresses
```

## KVM Integration

### KVM VM Creation

```
QEMU KVM Initialization:

1. Open KVM Device:
   fd = open("/dev/kvm", O_RDWR)
   └─> Get KVM capabilities

2. Create VM:
   vm_fd = ioctl(fd, KVM_CREATE_VM, type)
   ├─ Get VM file descriptor
   ├─ VM object created in kernel
   └─ Ready to configure

3. Allocate Memory:
   ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, {
       guest_phys_addr: 0,
       memory_size: guest_ram_size,
       userspace_addr: qemu_malloc_ptr,
   })
   └─> Setup guest physical → host virtual mapping

4. Create VCPUs:
   for each CPU:
       vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, cpu_id)
       ├─ Create VCPU file descriptor
       ├─ Allocate kernel VCPU state
       └─ Ready to run

5. Setup Interrupt Controller:
   ioctl(vm_fd, KVM_CREATE_IRQCHIP, NULL)
   └─> Create PIC/APIC in kernel (if x86)

6. Setup I/O Port Emulation:
   ioctl(vm_fd, KVM_SET_IOBITMAP_A, bitmap)
   └─> Routes certain I/O ports to QEMU
```

### VCPU Execution

```
Main VCPU Loop:

loop {
    1. Setup VCPU state:
       ioctl(vcpu_fd, KVM_SET_REGS, &regs)
       ioctl(vcpu_fd, KVM_SET_SREGS, &sregs)
       └─> Load CPU registers

    2. Enter guest mode:
       ioctl(vcpu_fd, KVM_RUN, NULL)
       ├─ Execute guest code
       ├─ Hardware CPU in guest mode
       └─ Blocked until VM Exit

    3. VM Exit (guest code stopped):
       ├─ Hardware raises exit
       └─ KVM returns control to QEMU

    4. Read exit reason:
       exit_reason = kvm_run->exit_reason
       ├─ IO_IN (input from port)
       ├─ IO_OUT (output to port)
       ├─ MMIO_READ (memory I/O read)
       ├─ MMIO_WRITE (memory I/O write)
       ├─ HLT (guest halted)
       ├─ SHUTDOWN (guest poweroff)
       └─ ... other exits

    5. Handle exit:
       switch(exit_reason) {
           case KVM_EXIT_IO:
               handle_io_instruction()
           case KVM_EXIT_MMIO:
               handle_mmio_instruction()
           ...
       }

    6. Loop back to #1
}
```

### Memory Mapping

```
KVM Memory Translation:

Guest Virtual Address (GVA)
    ↓ (Guest Page Table)
Guest Physical Address (GPA)
    ↓ (KVM Memory Slot)
Host Virtual Address (HVA)
    ↓ (Host Page Table)
Host Physical Address (HPA)
    ↓ (Hardware MMU)
Actual RAM

Optimization (EPT/NPT):
    Guest Virtual Address (GVA)
        ↓
    Guest Physical Address (GPA)
        ↓ (Extended Page Table - EPT or Nested Page Table - NPT)
    Host Physical Address (HPA)
        ↓ (Single lookup, hardware managed)
    Actual RAM

Benefits:
    ├─ Single MMU lookup (vs two)
    ├─ Faster TLB performance
    └─ Transparent shadow paging
```

## Hypervisor Backends

### HVF (Hypervisor Framework - macOS)

```
Architecture:
    Similar to KVM on Linux
    ├─ Uses Apple's Hypervisor.framework
    ├─ VMX (Virtual Machine Extension)
    ├─ EPT (Extended Page Tables)
    └─ Nested virtualization support

API:
    ├─ hv_vm_create() - Create VM
    ├─ hv_vcpu_create() - Create VCPU
    ├─ hv_vcpu_run() - Run VCPU
    ├─ hv_vcpu_get_state() - Read registers
    └─ hv_vcpu_set_state() - Write registers

Limitations:
    ├─ macOS only
    ├─ Requires hardware support (T2 chip or later)
    └─ Limited to Apple hardware
```

### WHPX (Windows Hypervisor Platform)

```
Architecture:
    Microsoft's hypervisor interface for Windows 10/11
    ├─ Similar to KVM/HVF
    ├─ Uses Hyper-V infrastructure
    ├─ VPCPU threads interact with Hyper-V
    └─ Requires Hyper-V capable hardware

API:
    ├─ WHvCreatePartition() - Create VM
    ├─ WHvCreateVirtualProcessor() - Create VCPU
    ├─ WHvRunVirtualProcessor() - Run VCPU
    ├─ WHvGetVirtualProcessorRegisters() - Read registers
    └─ WHvSetVirtualProcessorRegisters() - Write registers

Limitations:
    ├─ Windows 10/11 only
    ├─ Requires Hyper-V capable CPU (Intel/AMD)
    └─ Cannot run alongside other hypervisors on same hardware
```

## Machine-Specific Features

### ACPI (Advanced Configuration and Power Interface)

```
ACPI Functionality in VMs:

1. Power Management:
   ├─ Hotplug notification
   ├─ Sleep/wake states
   ├─ Power off support
   └─ Battery status (for laptops)

2. Device Discovery:
   ├─ Dynamic device enumeration
   ├─ Device hotplug events
   └─ Soft device removal

3. Interrupt Routing:
   ├─ IRQ assignment
   ├─ Interrupt priorities
   └─ Conflict resolution

QEMU ACPI Implementation:
    ├─ ACPI tables in BIOS/UEFI
    ├─ AML (ACPI Machine Language) bytecode
    ├─ Dynamically generated tables
    └─ Hotplug event generation

Example: ACPI Hotplug Notification
    Device hotplug request
        ↓
    QEMU generates ACPI event
        ↓
    Guest receives SCI (System Control Interrupt)
        ↓
    ACPI handler reads event
        ↓
    Enumeration code discovers new device
        ↓
    Device driver loaded
```

### SMM (System Management Mode) - x86

```
SMM Purpose:
    ├─ CPU enters privileged mode
    ├─ Outside guest OS control
    ├─ Used for power management
    ├─ System-level event handling
    └─ Firmware updates (in theory)

QEMU SMM Support:
    ├─ Partial emulation
    ├─ Some instructions handled
    ├─ State saved/restored
    └─ Limited functionality

Use Cases:
    ├─ PowerButton handling
    ├─ Fan control
    └─> Some BIOS functions
```

### Memory Hotplug

```
Hot-Adding Memory:

1. Command:
   object_add memory-backend-ram,size=1G,id=mem0
   device_add pc-dimm,memdev=mem0,id=dimm0,slot=0

2. QEMU Processing:
   ├─ Allocate memory region
   ├─ Add to memory map
   ├─ Notify guest via ACPI
   └─> Update guest DSDT

3. Guest OS:
   ├─ Receives hotplug event
   ├─ Enumerates new memory
   ├─ Onlines memory pages
   └─> Applications can use

4. Result:
   ├─ Guest RAM increased at runtime
   ├─ No reboot required
   └─ Useful for dynamic workloads
```

## Device Tree Architecture

### Dynamic Device Tree Generation

```
QEMU Device Tree Creation:

1. Start with base device tree (board-specific)
2. Add CPUs based on command-line (-smp)
3. Add memory based on command-line (-m)
4. Add devices based on devices added (-device)
5. Generate final DTB
6. Pass to guest kernel

Example ARM Board:

qemu-system-arm -machine virt -m 1024 -smp 2 \
    -device e1000,netdev=net0 \
    -netdev user,id=net0 \
    -drive file=disk.img,format=raw,if=virtio

Resulting Device Tree:
├─ /cpus
│  ├─ /cpu@0 (2 CPUs due to -smp 2)
│  └─ /cpu@1
├─ /memory@80000000 (1GB due to -m 1024)
├─ Interrupt controller
├─ Timer
├─ /pcie@10000000
│  ├─ e1000 (added via -device)
│  └─ virtio-blk (added via -drive)
└─ UART
```

---

*See related documents: [Overview](01-overview.md), [Core Emulation Systems](02-core-emulation.md)*
