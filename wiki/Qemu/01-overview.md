# QEMU Architecture Overview

## Table of Contents
1. [Introduction](#introduction)
2. [High-Level Architecture](#high-level-architecture)
3. [Key Components](#key-components)
4. [Execution Model](#execution-model)
5. [Architecture Diagram](#architecture-diagram)

## Introduction

QEMU (Quick Emulator) is a generic and open-source machine emulator and virtualizer. It provides:

- **Full-system emulation**: Complete machine emulation without hardware acceleration
- **User-space emulation**: Linux and BSD system call emulation
- **Hypervisor integration**: Support for KVM, Xen, HVF, and other accelerators

The QEMU architecture is designed to be:
- **Modular**: Each component has well-defined responsibilities
- **Portable**: Runs on Linux, Windows, macOS, and other UNIX variants
- **Extensible**: Supports adding new architectures, devices, and features
- **Performance-oriented**: Optimized for speed while maintaining compatibility

## High-Level Architecture

```
┌─────────────────────────────────────────────────┐
│         Guest Operating System / Application     │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│           Virtual Machine (Guest)                │
│  ┌─────────────────────────────────────┐       │
│  │   Virtual CPU (vCPU) Thread(s)      │       │
│  │  ┌─────────────────────────────┐    │       │
│  │  │  TCG / KVM / Accelerator    │    │       │
│  │  └─────────────────────────────┘    │       │
│  └─────────────────────────────────────┘       │
│  ┌─────────────────────────────────────┐       │
│  │   Virtual Devices                    │       │
│  │  (NICs, Disks, Consoles, etc.)      │       │
│  └─────────────────────────────────────┘       │
│  ┌─────────────────────────────────────┐       │
│  │   Virtual Memory & MMU               │       │
│  └─────────────────────────────────────┘       │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│            QEMU Core Runtime                     │
│  ┌────────────┐  ┌────────────┐  ┌──────────┐ │
│  │ QOM Model  │  │ Device Mgr │  │ Memory   │ │
│  │ (Objects)  │  │ (Bus)      │  │ Manager  │ │
│  └────────────┘  └────────────┘  └──────────┘ │
│  ┌────────────┐  ┌────────────┐  ┌──────────┐ │
│  │ I/O Thread │  │ Block Layer│  │ Monitor  │ │
│  └────────────┘  └────────────┘  └──────────┘ │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│           Host Operating System                  │
│  ┌─────────────────────────────────────┐       │
│  │  Host Kernel (KVM, Xen, etc.)       │       │
│  │  Threads, Memory, I/O Subsystems    │       │
│  └─────────────────────────────────────┘       │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│           Host Hardware                          │
│  ┌──────────────┐  ┌─────────┐  ┌────────────┐ │
│  │ CPU/Cores    │  │ Memory  │  │ I/O Devices│ │
│  └──────────────┘  └─────────┘  └────────────┘ │
└──────────────────────────────────────────────────┘
```

## Key Components

### 1. **CPU Acceleration Layer (accel/)**
- **TCG (Tiny Code Generator)**: Pure software CPU emulation via JIT compilation
- **KVM (Kernel-based Virtual Machine)**: Hardware virtualization support for Linux
- **HVF (Hypervisor Framework)**: macOS hypervisor support
- **Xen**: Xen hypervisor integration
- **HAX, WHPX**: Windows hypervisor support

### 2. **Machine & Architecture Support (hw/ and target/)**
- x86/x64, ARM, MIPS, PowerPC, RISC-V, Sparc, and more
- Machine types for different hardware configurations
- Board initialization and device assembly

### 3. **Device Model Framework (qom/ and qdev/)**
- **QOM (QEMU Object Model)**: Unified object hierarchy
- **QDEV**: Device abstraction and bus management
- Provides consistency across device implementations

### 4. **Memory Management (softmmu/)**
- Guest virtual memory to host virtual memory translation
- Translation Lookaside Buffer (TLB)
- IOMMU support for device memory access

### 5. **I/O and Device Emulation (hw/, block/, net/, etc.)**
- PCI, USB, SCSI bus implementations
- Device drivers: NIC, disk, serial console, timer
- Block I/O layer with format support (qcow2, raw, vmdk, etc.)

### 6. **System Services**
- **QMP (QEMU Machine Protocol)**: Remote management interface
- **HMP (Human Monitor Protocol)**: Interactive console
- Live migration and state management
- Debugging and tracing infrastructure

## Execution Model

### Single-Threaded Execution (Default)
```
Main Thread (QEMU Process)
│
├─ Event Loop (Main Event Loop)
│  ├─ Check for I/O events
│  ├─ Process monitor commands
│  ├─ Execute VCPUs in sequence
│  └─ Handle timers and callbacks
│
└─ Device Emulation
   ├─ PCI devices
   ├─ Storage controllers
   └─ Network interfaces
```

### Multi-Threaded Execution (TCG mode)
```
Main Thread              VCPU Thread 1        VCPU Thread 2
│                        │                    │
├─ Event Loop            ├─ TCG Loop          ├─ TCG Loop
│  ├─ I/O events         │  ├─ Translation    │  ├─ Translation
│  ├─ Monitor cmds       │  ├─ Execution      │  ├─ Execution
│  └─ Synchronization    │  └─ Halt           │  └─ Halt
│                        └─                   └─
└─ Device I/O & State Management
```

### KVM Acceleration
```
Host User Space              Host Kernel Space
│                            │
├─ QEMU Process              ├─ KVM Module
│  ├─ Main Thread            │  │
│  │  ├─ Device Emulation    │  ├─ Hardware Virtual CPU
│  │  └─ Monitor             │  ├─ MMU & Memory Paging
│  │                         │  └─ Interrupt Handling
│  └─ VCPU Thread(s)         │
│     ├─ ioctl(KVM_RUN)      │
│     └─ Handle Exits ◄─────►│
│                            │
└────────────────────────────┘
```

## Boot Process Flow

1. **Initialization Phase**
   - Parse command-line arguments
   - Initialize QOM and device model
   - Create virtual machine object
   - Register devices and machine type

2. **Machine Setup Phase**
   - Allocate guest memory
   - Initialize memory manager and TLB
   - Load BIOS/firmware
   - Initialize system devices (timer, serial, etc.)

3. **Device Assembly Phase**
   - Create PCI bus and devices
   - Set up interrupt routing
   - Initialize network and storage devices
   - Configure board-specific hardware

4. **Execution Phase**
   - Start main event loop
   - Launch VCPU threads
   - Begin guest code execution
   - Process guest I/O and device interrupts

## Core Design Patterns

### 1. Object Model (QOM)
- Hierarchical object system
- Property-based configuration
- Inheritance and composition
- Type introspection

### 2. Bus Architecture
- Standard bus implementations (PCI, USB, I2C, SPI)
- Bus-device relationship
- Hot-plug support
- Interrupt and DMA routing

### 3. Device State & Migration
- Loadable/saveable state
- Snapshot support
- Live migration capability
- Deterministic execution option

### 4. Virtio Devices
- Efficient guest-host communication
- Para-virtual device interface
- Rings and notification mechanism
- Support for network, block, console devices

## Concurrency & Synchronization

- **Global Mutex (BQL)**: Serializes QEMU core operations
- **RCU (Read-Copy-Update)**: Lock-free synchronization for data structures
- **Event-driven I/O**: Asynchronous device handling
- **Multi-threaded TCG**: Per-CPU threads with synchronization points

## Performance Optimization Techniques

1. **Instruction Translation Caching**: TCG blocks cached after translation
2. **TLB Caching**: Virtual-to-physical address translations cached
3. **Device Model Optimization**: Lazy device emulation, batching
4. **Hardware Acceleration**: KVM for full CPU virtualization
5. **I/O Optimization**: Asynchronous I/O and batching
6. **Memory Optimization**: Copy-on-write, memory deduplication support

## State Management

- **Snapshots**: Point-in-time VM state capture
- **Live Migration**: Transfer running VM between hosts
- **Replay/Record**: Deterministic execution for debugging
- **Savevm/Loadvm**: Persistent state management

---

*See related documents: [Core Emulation Systems](02-core-emulation.md), [Memory Management](03-memory-management.md), [Device Model Architecture](04-device-model.md)*
