# Storage and Block I/O Architecture

## Table of Contents
1. [Block Layer Overview](#block-layer-overview)
2. [Block Drivers](#block-drivers)
3. [Block Backends](#block-backends)
4. [Storage Controllers](#storage-controllers)
5. [I/O Request Handling](#io-request-handling)
6. [Disk Image Formats](#disk-image-formats)
7. [Snapshots and Overlays](#snapshots-and-overlays)

## Block Layer Overview

The block layer provides a uniform interface for disk I/O operations, abstracting various storage backends.

### Block Layer Architecture

```
┌─────────────────────────────────────────┐
│   Guest Operating System                │
│  (Disk read/write requests)             │
└────────────────┬────────────────────────┘
                 │
┌────────────────▼────────────────────────┐
│ Storage Controller Driver                │
│ (AHCI, SCSI, NVMe, Virtio-BLST, etc.)   │
└────────────────┬────────────────────────┘
                 │
┌────────────────▼────────────────────────┐
│ QEMU Block Layer API                     │
│ ├─ Request queue management              │
│ ├─ Completion handling                   │
│ └─ Error handling                        │
└────────────────┬────────────────────────┘
                 │
        ┌────────┼────────┐
        │        │        │
        ▼        ▼        ▼
    ┌────────┐ ┌────────┐ ┌────────┐
    │ Format │ │Protocol│ │  I/O   │
    │Drivers │ │ Stack  │ │Engines │
    ├────────┤ ├────────┤ ├────────┤
    │ qcow2  │ │  NBD   │ │ RawIO  │
    │ vmdk   │ │  SSH   │ │ AIO    │
    │ vpc    │ │ iSCSI  │ │ Thread │
    │ raw    │ │ HTTPS  │ │  Pool  │
    └────────┘ └────────┘ └────────┘
        │        │        │
        └────────┼────────┘
                 │
    ┌────────────▼──────────────┐
    │ Physical Storage Backend    │
    │ ├─ File (raw disk image)    │
    │ ├─ Block Device             │
    │ ├─ Network (NBD, iSCSI)     │
    │ └─ RAM (Memory disk)        │
    └─────────────────────────────┘
```

## Block Drivers

Block drivers implement the actual disk I/O operations.

### BlockDriver Structure

```c
typedef struct BlockDriver {
    // Driver identification
    const char *format_name;
    int instance_size;
    
    // Capabilities flags
    int coroutine_fn;
    int is_filter;
    
    // Initialization
    int (*bdrv_probe)(const uint8_t *buf, int buf_size, 
                      const char *filename);
    int (*bdrv_open)(BlockDriverState *bs, QDict *options, int flags);
    int (*bdrv_close)(BlockDriverState *bs);
    
    // I/O Operations
    int (*bdrv_pread)(BlockDriverState *bs, int64_t offset,
                      uint8_t *buf, int count);
    int (*bdrv_pwrite)(BlockDriverState *bs, int64_t offset,
                       uint8_t *buf, int count);
    
    // Async I/O
    BlockAIOCB *(*bdrv_aio_readv)(BlockDriverState *bs,
                                  int64_t sector_num, QEMUIOVector *iov,
                                  BlockCompletionFunc *cb, void *opaque);
    BlockAIOCB *(*bdrv_aio_writev)(BlockDriverState *bs,
                                   int64_t sector_num, QEMUIOVector *iov,
                                   BlockCompletionFunc *cb, void *opaque);
    
    // Other operations
    int (*bdrv_flush)(BlockDriverState *bs);
    int (*bdrv_truncate)(BlockDriverState *bs, int64_t offset);
    
    // Snapshots
    int (*bdrv_snapshot_create)(BlockDriverState *bs, QEMUSnapshotInfo *sn_info);
    int (*bdrv_snapshot_goto)(BlockDriverState *bs, const char *snapshot_id);
    
} BlockDriver;
```

### Common Block Drivers

```
Raw Format:
├─ Direct file/block device access
├─ No metadata or formatting
└─ Fastest I/O (minimal overhead)

QCOW2 (QEMU Copy-On-Write):
├─ Sparse format
├─ Efficient snapshots
├─ In-image metadata
└─ Compression support

VMDK (VMware Disk Format):
├─ VMware compatibility
├─ Split descriptor/data files
└─ Sparse allocation

VPC (Virtual Hard Disk):
├─ Hyper-V/VirtualBox format
├─ Dynamic allocation
└─ Snapshot support

NBD (Network Block Device):
├─ Network-based backend
├─ Remote storage access
└─> Actual I/O performed on remote server

RBD (RADOS Block Device):
├─ Ceph storage support
├─ Distributed storage
└─> Data spread across cluster

iSCSI:
├─ SCSI over network
├─ Enterprise storage
└─> Connected to remote storage array
```

## Block Backends

Block backends handle the actual storage device I/O.

### I/O Engines

```
Synchronous I/O (Blocking):
    ├─ read() / write() system calls
    ├─ Thread blocks until complete
    ├─ Simple but can block event loop
    └─ Good for: Low latency, single-threaded

Asynchronous I/O (AIO):
    ├─ Non-blocking I/O operations
    ├─ Callbacks on completion
    ├─ Scalable for many concurrent requests
    └─ Good for: High throughput

Thread Pool:
    ├─ Dedicated I/O thread pool
    ├─ Offload blocking operations
    ├─ Prevents event loop blocking
    └─ Good for: Block devices, remote storage

Linux native AIO:
    ├─ Kernel AIO (aio_read, aio_write)
    ├─ Zero-copy operations
    ├─ Direct to block device
    └─ Good for: Block devices, raw images

io_uring (newer):
    ├─ Newer Linux kernel interface
    ├─ Batched I/O operations
    ├─ Lower overhead
    └─ Good for: High performance storage
```

### Backend Selection Flow

```
1. User specifies backend via command line:
   -drive file=disk.qcow2,aio=threads,cache=writeback

2. QEMU Backend Negotiation:
   ├─ Requested backend: threads
   ├─ Try thread pool first
   ├─ If unavailable, fallback to sync
   └─> Verify driver support

3. Backend Initialization:
   ├─ Allocate I/O resources
   ├─ Setup thread pools (if async)
   ├─ Configure buffering
   └─> Ready for I/O

4. I/O Request Handling:
   ├─ Guest issues disk I/O
   ├─ QEMU routes to backend
   ├─ Backend handles transfer
   └─> Callback on completion
```

## Storage Controllers

Storage controllers manage the connection between the guest and disks.

### Controller Types

```
AHCI (Advanced Host Controller Interface):
    ├─ SATA disk support
    ├─ Multiple port support (up to 32)
    ├─ Native Command Queuing (NCQ)
    └─ Modern standard controller

IDE (Integrated Drive Electronics):
    ├─ Legacy ATA controller
    ├─ Two channels, 2 devices each
    ├─ Slower but ubiquitous
    └─ Good for compatibility

SCSI (Small Computer System Interface):
    ├─ Enterprise disk protocol
    ├─ Many devices per controller
    ├─ Advanced features (LUN support)
    └─ Used in servers

NVMe (Non-Volatile Memory Express):
    ├─ Modern high-speed protocol
    ├─ Supports PCIe NVMe devices
    ├─ Very high throughput
    └─ Low latency

USB Mass Storage:
    ├─ USB device emulation
    ├─ External disk simulation
    └─ SCSI protocol over USB

Virtio-Block:
    ├─ Para-virtual device
    ├─ Efficient guest-host communication
    └─ Preferred for Linux guests
```

### Controller Architecture

```
Storage Controller (e.g., AHCI):

PCI Device
├─ Vendor/Device ID
├─ Memory-Mapped I/O Registers
├─ Interrupt handling
└─ DMA support

Per-Port State:
├─ Port status registers
├─ Command list pointer
├─ Received FIS buffer pointer
├─ Connection status
└─ Queue management

Command Processing:
1. Guest prepares command in memory
2. Guest writes command list address to controller
3. Controller reads command descriptor
4. Controller executes command
5. Controller writes completion status
6. Controller generates interrupt
7. Guest reads status and processes completion
```

## I/O Request Handling

### Request Flow

```
1. Guest Issues I/O Request:
   Guest Driver → Controller Driver
   ├─ Prepare command descriptor
   ├─ Setup data buffer pointers
   ├─ Write command to queue
   └─> Notify controller (doorbell/register write)

2. QEMU Intercepts Request:
   Controller Emulation
   ├─ Monitor command register
   ├─ Decode command
   ├─ Translate guest addresses to host
   └─> Issue to block backend

3. Backend Processing:
   I/O Engine
   ├─ Read/write from storage backend
   ├─ Handle errors if any
   └─> Prepare completion notification

4. Request Completion:
   QEMU → Guest
   ├─ Write completion status
   ├─ Update status registers
   ├─ Generate interrupt (if configured)
   └─> Guest reads result

5. Guest Completes:
   Guest Driver
   ├─ Process completion status
   ├─ Validate data
   ├─ Return to guest OS
   └─> Ready for next I/O
```

### Scatter-Gather (SG) List

```
For multi-buffer I/O operations:

Command Structure:
├─ SG List Address
├─ Number of SG Entries
└─ Transfer Length

SG Entry (physical address + length):
├─ [63:0]  Buffer Address
└─ [31:16] Buffer Length

Example: Read 64KB from two buffers
Entry 0: Address=0x1000, Length=0x2000 (8KB)
Entry 1: Address=0x8000, Length=0x6000 (24KB)

Total Transfer: 32KB (not contiguous in guest memory)
```

## Disk Image Formats

### Raw Format

```
Structure:
    [Disk data - no metadata]
    
Characteristics:
    ├─ Direct mapping of guest disk to host file
    ├─ Fastest performance
    ├─ No compression or snapshots
    ├─ Not sparse (allocates full size)
    └─ Size: ~100GB disk = 100GB file

Usage:
    -drive file=disk.raw,format=raw
```

### QCOW2 Format

```
File Structure:
┌─────────────────────────────┐
│  Header (72 bytes)          │
│  ├─ Magic number            │
│  ├─ Format version          │
│  ├─ Backing file info       │
│  ├─ Encryption              │
│  └─ Snapshot info           │
├─────────────────────────────┤
│  L1 Table (variable)        │ ← Points to L2 tables
│  (Cluster pointers)         │
├─────────────────────────────┤
│  L2 Tables (variable)       │ ← Points to data clusters
│  (Cluster pointers)         │
├─────────────────────────────┤
│  Refcount Table (variable)  │ ← Reference counting
│  (Cluster references)       │
├─────────────────────────────┤
│  Data Clusters (variable)   │ ← Actual disk data
│  (4KB - 2MB each)           │
└─────────────────────────────┘

Features:
    ├─ Sparse allocation (only used space takes space)
    ├─ Snapshots (COW backing chain)
    ├─ Compression support
    ├─ Encryption support
    ├─ Dirty bitmap for backup
    └─ In-image metadata

Performance:
    ├─ Read: Slight overhead (table lookup)
    ├─ Write: COW overhead (copy-on-write)
    └─ Typical: 5-15% performance reduction vs raw

Example Sizes:
    ├─ 100GB virtual disk (10% used) = 10GB file
    ├─ 100GB virtual disk (100% used) = 100GB file
    └─ Benefits for: Sparse disks, testing, archival
```

## Snapshots and Overlays

### Snapshot Architecture

```
Backing Chain (COW Model):

base.qcow2
├─ Immutable (in snapshot mode)
├─ Original disk state
└─ Size: 100GB (but only 10GB used)

snap1.qcow2 (overlay on base.qcow2)
├─ Copy-on-write changes
├─ Tracks modifications since base
└─ Size: ~5GB (only changed blocks)

snap2.qcow2 (overlay on snap1.qcow2)
├─ Further modifications
├─ Independent from snap1 after creation
└─ Size: ~2GB (only new changes)

Guest View:
    guest_disk = snap2 + snap1 + base
    (Stacked transparently)
```

### Snapshot Operations

```
Creating Snapshot:
1. Mark current image as backing chain
2. Create new empty overlay
3. Redirect writes to new overlay
4. Keep old image immutable
5. New writes don't affect snapshot

Example:
    qemu-img snapshot -c snap1 disk.qcow2
    
    Before:
    └─ disk.qcow2 (100% data + metadata)
    
    After:
    ├─ disk.qcow2 (backing image, read-only)
    └─ disk.qcow2 overlay (new writes)

Reverting Snapshot:
1. Discard overlay
2. Revert to backing image state
3. Lost all changes since snapshot

Merging Snapshots:
1. Commit overlay to backing image
2. Combines changes
3. Reduces chain length

Chain Limits:
    ├─ Longer chains = more overhead
    ├─ Each level adds read latency
    ├─ Typical limit: 3-5 levels
    └─ Can rebase/consolidate to improve performance
```

---

*See related documents: [Device Model Architecture](04-device-model.md), [Advanced Topics](10-advanced-topics.md)*
