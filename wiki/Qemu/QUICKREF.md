# QEMU Architecture Quick Reference

## Document Index

| Doc | Title | Purpose | Key Topics |
|-----|-------|---------|-----------|
| [01](01-overview.md) | Architecture Overview | Foundation and high-level design | Components, execution models, design patterns |
| [02](02-core-emulation.md) | Core Emulation Systems | CPU emulation engines | TCG, KVM, instruction translation, TLB |
| [03](03-memory-management.md) | Memory Management | Virtual memory and addressing | Address translation, memory regions, IOMMU |
| [04](04-device-model.md) | Device Model Architecture | Object-oriented device framework | QOM, QDEV, properties, lifecycle |
| [05](05-pci-bus-architecture.md) | PCI and Bus Architecture | PCI subsystem and bus management | PCI configuration, BARs, interrupts, hotplug |
| [06](06-storage-block-io.md) | Storage and Block I/O | Disk I/O subsystem | Block drivers, controllers, snapshots |
| [07](07-network-virtio.md) | Network and Virtio | Network devices and para-virtualization | Network backends, virtio devices, I/O rings |
| [08](08-machine-hypervisor.md) | Machine and Hypervisor | Machine types and VM acceleration | Machine types, firmware, KVM, accelerators |
| [09](09-io-interrupts.md) | I/O and Interrupt Handling | IRQ management and I/O emulation | PIC, APIC, GIC, I/O ports, MMIO |
| [10](10-advanced-topics.md) | Advanced Topics | VM lifecycle and optimization | Migration, snapshots, debugging, performance |

## Architecture Layers

```
┌────────────────────────────────────────────────────────┐
│  Layer 10: Advanced Features                            │
│  ├─ Live Migration ├─ Snapshots ├─ Debugging ├─ Perf  │
└────────────┬───────────────────────────────────────────┘
             │
┌────────────▼────────────────────────────────────────────┐
│  Layer 9: I/O and Interrupts                            │
│  ├─ PIC ├─ APIC ├─ GIC ├─ I/O Ports ├─ MMIO            │
└────────────┬───────────────────────────────────────────┘
             │
┌────────────▼───────────────────────────────────────────┐
│  Layer 8: Machine & Hypervisor                         │
│  ├─ Firmware ├─ KVM ├─ Machine Types ├─ Accelerators  │
└────────────┬──────────────────────────────────────────┘
             │
    ┌────────┼────────┬────────────────┐
    │        │        │                │
    ▼        ▼        ▼                ▼
┌────────┐┌────────┐┌────────┐   ┌──────────┐
│Layer 7:│ │Layer 6:│ │Layer 5:│   │  Layer 4:│
│Network │ │Storage │ │PCI/Bus │   │ Device  │
│Devices │ │& I/O   │ │Arch    │   │ Model   │
└────────┘ └────────┘ └────────┘   └──────────┘
    │         │         │              │
    └─────────┼─────────┴──────────────┘
              │
    ┌─────────▼──────────────────────┐
    │  Layer 3: Memory Management    │
    │  ├─ TLB ├─ Paging ├─ Regions  │
    └─────────┬──────────────────────┘
              │
    ┌─────────▼──────────────────────┐
    │  Layer 2: CPU Emulation        │
    │  ├─ TCG ├─ KVM ├─ Instruction │
    └─────────┬──────────────────────┘
              │
    ┌─────────▼──────────────────────┐
    │  Layer 1: Guest OS/Application │
    └───────────────────────────────┘
```

## Component Relationships

```
Device Model Hierarchy:

Object (QOM)
│
├─ Machine
│  └─ Contains: CPUs, Buses, Devices
│
├─ CPU
│  └─ Contains: Registers, State, TLB
│
├─ Device
│  ├─ Properties (configuration)
│  ├─ MemoryRegions (I/O, memory-mapped)
│  ├─ IRQs (interrupts)
│  └─ State (registers, buffers)
│
├─ Bus
│  └─ Contains: Child Devices
│
└─ Backend
   └─ (Network, storage, character device)
```

## Key Data Flow Paths

### Guest to Host I/O

```
Guest Application (write to disk)
    ↓
Guest OS (syscall dispatcher)
    ↓
Storage Controller Driver (AHCI, Virtio)
    ↓
Controller Emulation (QEMU)
    ↓
Block Layer (QEMU)
    ↓
Block Driver (qcow2, raw)
    ↓
I/O Backend (file, block device)
    ↓
Host Filesystem/Device
```

### Host to Guest Interrupt

```
External Event (Packet arrival, disk completion)
    ↓
Device Model (NIC, AHCI)
    ↓
Interrupt Controller (PIC, APIC)
    ↓
CPU Exception (INT xx)
    ↓
Guest IDT Handler
    ↓
Guest Driver ISR
    ↓
Guest Interrupt Processing
```

## Selection Guides

### Choosing CPU Acceleration

```
┌─────────────────────────────────┐
│  Which Accelerator?             │
├─────────────────────────────────┤
│  Linux + VT-x/SVM → KVM         │
│  macOS → HVF                    │
│  Windows 10/11 → WHPX           │
│  All platforms → TCG            │
│  Android (x86) → HCX/HAXM       │
└─────────────────────────────────┘

Performance: KVM/HVF > WHPX > TCG
Compatibility: TCG > others
```

### Choosing Storage Backend

```
┌──────────────────────────────────┐
│  Which Block Driver?             │
├──────────────────────────────────┤
│  Best speed → raw                │
│  Snapshots → qcow2               │
│  VMware compat → vmdk            │
│  Hyper-V compat → vpc            │
│  Network storage → nbd, iscsi    │
│  Ceph cluster → rbd              │
└──────────────────────────────────┘

Performance: raw > qcow2 > vmdk > vpc
Features: qcow2 > others
Compatibility: vmdk (VMware), vpc (Hyper-V)
```

### Choosing Network Device

```
┌──────────────────────────────────┐
│  Which Network Device?           │
├──────────────────────────────────┤
│  Linux guests → Virtio-Net       │
│  Windows guests → E1000/E1000e   │
│  High perf Linux → Virtio-Net MQ │
│  Compatibility → RTL8139         │
│  Minimal overhead → Virtio-Net   │
└──────────────────────────────────┘

Performance: Virtio-Net > E1000 > RTL8139
Compatibility: RTL8139 > E1000 > Virtio-Net
Recommended: Virtio-Net for Linux, E1000 for Windows
```

## Performance Tuning Checklist

- [ ] Use KVM/HVF acceleration (if available)
- [ ] Pin vCPUs to physical cores (CPU affinity)
- [ ] Enable huge pages (2MB or 1GB)
- [ ] Use Virtio devices for Linux guests
- [ ] Enable network MTU jumbo frames
- [ ] Use qcow2 for development, raw for production
- [ ] Enable I/O threads for high-throughput workloads
- [ ] Use multi-queue Virtio-Net for multi-core guests
- [ ] Disable unnecessary devices and features
- [ ] Monitor and profile with perf/tracing

## Common Commands Reference

```bash
# Start VM with detailed configuration
qemu-system-x86_64 \
  -accel kvm \
  -cpu host,+smep,+smap \
  -m 4G \
  -smp 4,cores=2,threads=2 \
  -drive file=disk.qcow2,format=qcow2,cache=writeback \
  -net nic,model=virtio \
  -net user,hostfwd=tcp:127.0.0.1:5022-:22 \
  -display gtk,gl=on

# Enable debugging
qemu-system-x86_64 -gdb tcp::1234 -S ...

# Create snapshot
qemu> savevm snapshot_name

# Enable tracing
qemu-system-x86_64 -trace enable=qemu_clock_* ...

# Record execution
qemu-system-x86_64 -record file=record.bin ...

# Replay execution  
qemu-system-x86_64 -replay file=record.bin ...
```

## Architecture Strengths

- **Modularity**: Clean separation of concerns across layers
- **Portability**: Runs on many host OS/architectures
- **Compatibility**: Supports diverse guest OS and architectures
- **Performance**: Multiple acceleration backends available
- **Extensibility**: Easy to add new devices, architectures
- **Debugging**: Comprehensive GDB and tracing support

## Common Bottlenecks & Solutions

| Bottleneck | Cause | Solution |
|-----------|-------|----------|
| High CPU usage | TCG emulation | Use KVM/HVF |
| Memory thrashing | TLB misses | Enable huge pages |
| Disk I/O slow | qcow2 overhead | Use raw format or cache tuning |
| Network latency | Interrupt overhead | Enable multi-queue |
| Frequent page faults | Memory pressure | Allocate more RAM |
| Context switching | No CPU affinity | Pin vCPUs to cores |

---

**Last Updated**: 2026
**QEMU Version**: Latest
**For Questions**: Refer to specific architecture documents or QEMU documentation
