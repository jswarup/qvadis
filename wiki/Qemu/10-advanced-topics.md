# Advanced Topics

## Table of Contents
1. [Live Migration](#live-migration)
2. [Snapshotting and State Management](#snapshotting-and-state-management)
3. [Record and Replay](#record-and-replay)
4. [Multi-threading](#multi-threading)
5. [Performance Optimization](#performance-optimization)
6. [Debugging Support](#debugging-support)
7. [Security Features](#security-features)

## Live Migration

Live migration allows moving a running VM from one host to another without stopping it.

### Migration Architecture

```
Source Host                           Destination Host
┌──────────────────────┐             ┌──────────────────────┐
│  VM Running          │             │  Empty VM            │
│  ├─ vCPU execution   │────Migrate──│  ├─ VM initialized   │
│  ├─ Memory state     │─────data───→│  ├─ Memory allocated │
│  ├─ Device state     │             │  ├─ Devices ready    │
│  └─ Dirty pages      │             │  └─ Waiting for state│
└──────────────────────┘             └──────────────────────┘
           │                                    │
           └─────────────────────────┬──────────┘
                       Synchronization
                      (freezes VM)
```

### Migration Steps

```
Phase 1: Setup
    ├─ Connect migration channel (TCP, UNIX socket, etc.)
    ├─ Initialize destination VM
    ├─ Verify compatibility (CPU, machine type, devices)
    └─> Prepare for transfer

Phase 2: Iterative Page Transfer
    Loop:
        ├─ Track dirty pages (pages modified since last transfer)
        ├─ Transfer dirty pages
        ├─ Mark pages clean
        ├─ Continue guest execution
        └─ Repeat until dirty rate < transfer rate

    Optimization:
        ├─ Transfer only changed pages
        ├─ Compression support
        ├─ XBZip compression for data
        └─> Reduces bandwidth

Phase 3: Stop-and-Copy
    ├─ Stop guest execution (pause all vCPUs)
    ├─ Transfer remaining dirty pages
    ├─ Transfer device state (PCI, controllers, etc.)
    ├─ Transfer CPU state (registers, flags)
    └─> Ensure no additional modifications

Phase 4: Activation
    ├─ Disconnect source VM
    ├─ Resume guest on destination
    ├─ Verify functionality
    └─> Migration complete

Total Downtime: 50-500ms (typically)
```

### State Saved During Migration

```
CPU State:
    ├─ General purpose registers (RAX, RBX, etc.)
    ├─ Control registers (CR0-CR4)
    ├─ Segment registers (CS, DS, SS, etc.)
    ├─ MMU state (Page tables, TLB)
    ├─ Floating point state
    ├─ Condition codes
    └─ Instruction pointer

Memory State:
    ├─ Guest RAM contents
    ├─ Dirty page tracking
    ├─ Memory mapping information
    └─ IOMMU translations (if present)

Device State:
    ├─ Device registers
    ├─ Device memory buffers
    ├─ Queue state (if applicable)
    ├─ Interrupt state
    └─ Statistics

VM Metadata:
    ├─ Machine type
    ├─ Device configuration
    ├─ BIOS/firmware settings
    └─ Boot parameters
```

## Snapshotting and State Management

### Snapshot Types

```
Memory Snapshot:
    ├─ Guest RAM + device state
    ├─ Disk: Not included
    ├─ Size: ~1GB per GB of guest RAM
    ├─ Restore: Quick (seconds)
    └─ Use: Testing, rollback

Disk Snapshot:
    ├─ Disk state at point in time
    ├─ Guest RAM: Not included
    ├─ Size: Usually sparse (only changes)
    ├─ Restore: Modify disk backing chain
    └─ Use: Version control, incremental backup

Full Snapshot (Savevm):
    ├─ Both memory AND disk
    ├─ Complete VM state
    ├─ Size: Large (RAM + disk changes)
    ├─ Restore: Restore all components
    └─ Use: Complete backup, disaster recovery
```

### Savevm Format

```
Savevm File Structure:

┌───────────────────────────────────┐
│ Magic Number & Version            │
├───────────────────────────────────┤
│ Timestamp                         │
├───────────────────────────────────┤
│ Machine Configuration              │
│ ├─ CPU type                       │
│ ├─ Memory size                    │
│ ├─ Number of devices              │
│ └─ Device configuration           │
├───────────────────────────────────┤
│ Device State Sections              │
│ ├─ APIC state                     │
│ ├─ PCI bus state                  │
│ ├─ AHCI controller state          │
│ ├─ NIC state                      │
│ └─ Other device state             │
├───────────────────────────────────┤
│ CPU State                         │
│ ├─ Registers                      │
│ ├─ Memory pages                   │
│ ├─ TLB entries                    │
│ └─ Floating point state           │
├───────────────────────────────────┤
│ RAM Pages                         │
│ (Compressed or raw)               │
├───────────────────────────────────┤
│ Checksum                          │
└───────────────────────────────────┘
```

### Snapshot Commands

```
Create Snapshot:
    savevm snapshot_name
    └─> Saves VM state to disk
    
List Snapshots:
    info snapshots
    └─> Shows all snapshots

Load Snapshot:
    loadvm snapshot_name
    ├─ Pause current VM
    ├─ Load saved state
    ├─ Resume execution
    └─> VM at previous point

Delete Snapshot:
    delvm snapshot_name
    └─> Removes snapshot from disk
```

## Record and Replay

Record and replay enables deterministic execution for debugging.

### Recording

```
Record Mode Operation:

1. Enable Recording:
   -record file=record.qemu
   
2. During Execution:
   ├─ Log all I/O operations
   ├─ Log all interrupts
   ├─ Log all randomness (RNG)
   ├─ Log all timers
   └─> Save to record file
   
3. Stop Recording:
   └─> Record file complete (all events logged)
   
Record File Contents:
    ├─ Instruction count between events
    ├─ Event type (interrupt, I/O, etc.)
    ├─ I/O operation (read/write/register)
    ├─ Data read/written
    ├─ Interrupt vector
    └─ Timestamp information
```

### Replay

```
Replay Mode Operation:

1. Enable Replay:
   -replay file=record.qemu
   
2. Execution with Replay:
   ├─ Execute instructions from TB cache
   ├─ At deterministic points, check event
   ├─ Inject recorded I/O result
   ├─ Inject recorded interrupt
   ├─ Verify execution matches original
   └─> Continue until replay file exhausted
   
3. Result:
   ├─ Identical execution path
   ├─ Same memory state
   ├─ Same register values
   ├─> Bit-for-bit reproducible
```

### Debugging with Record/Replay

```
Debugging Workflow:

1. Reproduce Bug (Record):
   qemu-system-x86_64 -record file=bug.record ...
   [Trigger bug]
   [Stop QEMU]
   
2. Debug (Replay):
   qemu-system-x86_64 -replay file=bug.record \
       -gdb tcp::1234
   [Connect GDB]
   gdb> continue           # Execution deterministic
   gdb> breakpoint
   [Step through, inspect state]
   
Benefits:
    ├─ Bug always reproducible
    ├─ Non-deterministic issues eliminated
    ├─ Step forward/backward
    └─> Easier debugging

Limitations:
    ├─ Large record files (up to GB)
    ├─ Performance overhead during record
    └─> Storage space required
```

## Multi-threading

### TCG Multi-threading

```
Default: Single-threaded TCG
    └─> All vCPUs share one thread
    └─> Simple but limited parallelism

MTTCG (Multi-Threaded TCG):
    ├─ One thread per vCPU
    ├─ Independent translation/execution
    ├─ Synchronization at memory barriers
    ├─ ~2-3x throughput on multicore
    └─> Enabled with -accel tcg,thread=multi

Synchronization:
    ├─ Global BQL (Big Kernel Lock) for critical sections
    ├─ Atomic operations for shared state
    ├─ Memory barriers for consistency
    └─> RCU (Read-Copy-Update) for data structures
```

### I/O Threading

```
Default (Single I/O thread):
    ├─ All I/O processed sequentially
    ├─ Simple but can block
    └─> Suitable for light I/O workloads

Multiple I/O Threads:
    ├─ Dedicated threads for I/O
    ├─ Parallel device handling
    ├─ Better throughput
    └─> For heavy I/O workloads

    -iothread id=io0
    -device virtio-blk,iothread=io0

Benefits:
    ├─ Non-blocking device I/O
    ├─ Parallel device operations
    ├─ Improved responsiveness
    └─> Better for VMs with many devices
```

## Performance Optimization

### CPU Affinity

```
vCPU to Physical CPU Binding:

Without Affinity:
    vCPU 0 ──┐
    vCPU 1  ├─→ OS scheduler
    vCPU 2  │  (can move between cores)
    vCPU 3 ──┘
    
    Issues:
    ├─ Context switch overhead
    ├─ Cache misses
    └─> Performance variance

With Affinity:
    vCPU 0 ──→ Core 0 (pinned)
    vCPU 1 ──→ Core 1 (pinned)
    vCPU 2 ──→ Core 2 (pinned)
    vCPU 3 ──→ Core 3 (pinned)
    
    Benefits:
    ├─ No context switching
    ├─ Better cache locality
    ├─ More predictable performance
    └─ 10-30% improvement typical

Configuration:
    -vcpu 4,affinity=0-3,cpuset=0-3
    or
    taskset -c 0-3 qemu-system-x86_64 ...
```

### Huge Pages

```
Standard 4KB Pages:
    ├─ 1M pages for 4GB memory
    ├─ More TLB pressure
    ├─ More page table lookups
    └─> Overhead: ~10% performance

Huge Pages (2MB):
    ├─ 2000 pages for 4GB memory
    ├─ Fewer TLB misses
    ├─ Faster address translation
    └─> Improvement: ~20-30%

Gigantic Pages (1GB):
    ├─ 4 pages for 4GB memory
    ├─ Minimal TLB misses
    ├─ Maximum performance
    └─> Improvement: ~30-50%

Configuration:
    -mem-prealloc -machine memory-backend=mem0
    (with memory-backend-file using huge pages)
```

### Network Optimization

```
Virtio-Net Tuning:

Interrupt Coalescing:
    ├─ Batch multiple packets before interrupt
    ├─ Reduces interrupt overhead
    ├─ Trade latency for throughput
    └─> Typical: ~30% throughput gain

Multi-queue:
    ├─ Multiple TX/RX rings
    ├─ Parallel packet processing
    ├─ Per-queue processing threads
    └─> 2-4x throughput improvement

Configuration:
    -device virtio-net,mq=on,vectors=10
    (10 MSI-X vectors for queues)
```

## Debugging Support

### GDB Stub Support

```
GDB Debugging:

1. Start QEMU with GDB server:
   qemu-system-x86_64 -gdb tcp::1234 ...
   
2. Connect with GDB:
   gdb vmlinux
   (gdb) target remote :1234
   
3. Debugging Commands:
   (gdb) break function_name
   (gdb) continue
   (gdb) step
   (gdb) next
   (gdb) info registers
   (gdb) x /20i $rip          # Disassemble
   (gdb) x /16wx $rsp         # Memory dump
   
4. Breakpoint in Guest Kernel:
   (gdb) break do_syscall
   (gdb) continue
   [Guest hits breakpoint]
   [Inspect guest state]
```

### Tracing Support

```
Event Tracing:

Enable Trace Points:
    -trace events=all
    -trace enable=qemu_clock_*
    -trace file=/tmp/trace.log
    
Output:
    trace_qemu_clock_enable time:1000.0
    trace_qemu_clock_disable time:1000.5
    trace_bdrv_aio_readv addr:0x1000 size:4096
    
Analysis:
    ├─ System performance analysis
    ├─ Identify bottlenecks
    ├─ Understand device behavior
    └─ Correlation with events
```

## Security Features

### Secure Boot

```
UEFI Secure Boot Support:

1. Enable UEFI:
   -drive file=OVMF_CODE.fd,if=pflash,unit=0,readonly=on
   
2. Secure Boot Configuration:
   ├─ Load signed BIOS
   ├─ Verify kernel signatures
   ├─ Check certificate validity
   └─> Prevent unauthorized code

Security Benefits:
    ├─ Prevent rootkits
    ├─ Ensure boot integrity
    ├─ Trust chain from firmware
    └─ Compatible with LUKS encryption
```

### Trusted Platform Module (TPM)

```
TPM Emulation:

Add TPM device:
    -tpmdev emulator,id=tpm0,chardev=chrtpm
    -chardev socket,id=chrtpm,path=/tmp/tpm.sock
    -device tpm-tis,tpmdev=tpm0
    
TPM Functions:
    ├─ Secure key storage
    ├─ Attestation
    ├─ Sealing data
    └─ Trusted platform verification
    
Use Cases:
    ├─ BitLocker encryption (Windows)
    ├─ Encrypted partition verification
    ├─ System integrity checking
    └─> Compliance with security policies
```

### SMEP/SMAP (Kernel Protection)

```
x86 Memory Protection Features:

SMEP (Supervisor Mode Execution Protection):
    ├─ Kernel cannot execute user pages
    ├─ Prevents privilege escalation
    └─ Enabled with -cpu feature=+smep

SMAP (Supervisor Mode Access Prevention):
    ├─ Kernel cannot access user data
    ├─ Requires explicit access enable
    ├─ More fine-grained protection
    └─ Enabled with -cpu feature=+smap
    
Impact:
    ├─ Prevents many privilege escalation exploits
    ├─ Small performance overhead (~1-5%)
    └─ Recommended for production
```

---

*See related documents: [Overview](01-overview.md), [Core Emulation Systems](02-core-emulation.md), [Storage and Block I/O](06-storage-block-io.md)*
