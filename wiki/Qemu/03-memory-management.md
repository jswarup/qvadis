# Memory Management Architecture

## Table of Contents
1. [Memory Model Overview](#memory-model-overview)
2. [Virtual Address Translation](#virtual-address-translation)
3. [TLB (Translation Lookaside Buffer)](#tlb-translation-lookaside-buffer)
4. [Memory Regions](#memory-regions)
5. [IOMMU Support](#iommu-support)
6. [Memory Optimization](#memory-optimization)

## Memory Model Overview

QEMU's memory system must handle multiple layers of address translation:

```
┌────────────────────────────────────────┐
│     Guest Application Code              │
│  (Uses Guest Virtual Addresses - GVA)   │
└────────────────────┬───────────────────┘
                     │
         ┌───────────▼──────────┐
         │  Guest Page Tables   │
         │  (Setup by Guest OS) │
         └───────────┬──────────┘
                     │
         ┌───────────▼──────────────────┐
         │  Guest Physical Address      │
         │  (GPA - inside guest memory) │
         └───────────┬──────────────────┘
                     │
         ┌───────────▼──────────────────┐
         │  QEMU Memory Region Handler  │
         │  (Maps GPA to HVA/HPA)       │
         └───────────┬──────────────────┘
                     │
         ┌───────────▼──────────────────┐
         │  Host Virtual Address (HVA)  │
         │  (QEMU process memory)       │
         └───────────┬──────────────────┘
                     │
         ┌───────────▼──────────────────┐
         │  Host Page Tables            │
         │  (Linux/OS kernel)           │
         └───────────┬──────────────────┘
                     │
         ┌───────────▼──────────────────┐
         │  Host Physical Address (HPA) │
         │  (Real system RAM)           │
         └───────────────────────────────┘
```

### Guest Memory Allocation

```
QEMU startup:
1. malloc() or mmap() large block of host memory (e.g., 4GB)
   └─> This becomes guest physical memory address space
   └─> Typically starting at HVA: 0x1000000

2. Map into guest address space as:
   ├─ Conventional memory (0x00000 - 0x9FFFF for x86)
   ├─ Extended memory (0x100000+)
   └─ Memory-mapped I/O regions

3. Setup initial page tables
   └─> Allow guest OS to manage virtual memory
```

## Virtual Address Translation

### x86 Example: 4-Level Paging

```
Guest Virtual Address (48-bit)
    │
    └─> [Level 4 Offset] [Level 3 Offset] [Level 2 Offset] [Level 1 Offset] [Page Offset]
            (9 bits)         (9 bits)        (9 bits)        (9 bits)       (12 bits)
    
    ▼
Guest Page Table Walk:
    1. Load CR3 (Page Table Base)
    2. Index Level 4 table using [L4 offset]
    3. Get Level 3 table address
    4. Index Level 3 table using [L3 offset]
    5. Get Level 2 table address
    6. Index Level 2 table using [L2 offset]
    7. Get Level 1 table address
    8. Index Level 1 table using [L1 offset]
    9. Get physical page address (PPN)
    10. Add page offset = Guest Physical Address (GPA)
    
    ▼
QEMU Translation:
    GPA -> HVA (via memory region lookup)
    └─> Perform actual memory access
```

### Page Fault Handling

```
Guest Application:
    ├─ Access address in unmapped page
    └─> Raises page fault exception

Guest CPU:
    ├─ Saves context
    └─> Invokes OS page fault handler

Guest OS (Page Fault Handler):
    ├─ Allocate physical page
    ├─ Setup page table entry
    └─> Resume faulting instruction

QEMU:
    ├─ Emulates page fault exception
    ├─ Guest OS updates page tables
    └─> Memory access now succeeds
```

## TLB (Translation Lookaside Buffer)

The TLB is a cache of virtual-to-physical address translations, dramatically improving performance.

### QEMU TLB Structure

```c
struct tlb_entry {
    target_ulong addr_read;      // Page start for read
    target_ulong addr_write;     // Page start for write
    target_ulong addr_code;      // Page start for code fetch
    
    target_ulong page_addr;      // Physical page address (or HVA)
    
    target_ulong addend;         // Quick translation: HVA = GPA + addend
    
    CPUTLBDescFast fast;         // Fast path data
    
    uint8_t prot;                // Protection bits (R/W/X)
    int32_t flags;               // Page type, permissions
};

struct CPUTLBContext {
    tlb_entry d[NB_MMU_MODES][CPU_TLB_SIZE];  // Multiple modes (user/kernel)
};
```

### TLB Lookup Performance

```
Fast Path (90%+ of lookups):
    1. Compute index: (virtual_addr >> PAGE_SHIFT) & TLB_MASK
    2. Load tlb_entry from array
    3. Check if addr_read >= page_addr && <= page_end
    4. Result: HVA = virtual_addr + addend
    ⏱ Typical: ~5 CPU cycles

Slow Path (TLB miss):
    1. Page table walk in guest address space
    2. Update TLB entry
    3. Retry memory access
    ⏱ Typical: ~50-200 CPU cycles
```

### TLB Invalidation

```
TLB Flush Scenarios:
    1. Context switch (TLB becomes invalid)
       └─> Flush on CR3 write (x86)
    
    2. Page table modification
       └─> Flush matching entries on INVLPG instruction
    
    3. Mode change (ring 0/3)
       └─> Flush if necessary for security
    
    4. Selective flush (TLB shootdown in multi-CPU)
       └─> Synchronize between VCPUs

Cost of Flush:
    - Full flush: ~1000-5000 cycles
    - Selective: ~100-500 cycles
    - Partial: ~10-100 cycles
```

## Memory Regions

Memory regions provide a flexible abstraction for memory-mapped I/O and device emulation.

### MemoryRegion Structure

```c
struct MemoryRegion {
    Object parent_obj;
    const MemoryRegionOps *ops;    // Read/write operations
    
    void *opaque;                  // Pointer to device state
    void *owner;                   // Owner device
    
    MemoryRegion *parent;          // Hierarchical parent
    Int128 size;                   // Region size
    hwaddr addr;                   // Address in parent
    
    uint8_t enabled;               // Is this region enabled?
    uint8_t romd_mode;             // ROM mode (read-only)
    bool ram;                       // Is RAM region?
    
    struct list dirty_pages;       // Dirty pages for migration
};
```

### MemoryRegionOps

```c
typedef struct MemoryRegionOps {
    // Read operations
    uint64_t (*read)(void *opaque, hwaddr addr, unsigned size);
    
    // Write operations
    void (*write)(void *opaque, hwaddr addr, uint64_t data, unsigned size);
    
    // Endianness
    bool endianness;
    
    // Atomicity
    bool atomic;
} MemoryRegionOps;
```

### Memory Map Example (x86)

```
Guest Physical Address Space (GPA)
┌─────────────────────────────────────────┐
│                                         │
│  0xFFFFFFFF ┌─────────────────────────┐ │
│             │    BIOS ROM             │ │ 64KB
│  0xFFFE0000 ├─────────────────────────┤ │
│             │  Video RAM (if enabled) │ │ 64KB
│  0xFFFD0000 ├─────────────────────────┤ │
│             │  (free)                 │ │
│  0x000F0000 ├─────────────────────────┤ │
│             │  VGA Video Memory       │ │ 64KB
│  0x000A0000 ├─────────────────────────┤ │
│             │  (free)                 │ │
│  0x00010000 ├─────────────────────────┤ │
│             │  Extended Memory (RAM)  │ │ Many MB
│  0x00100000 ├─────────────────────────┤ │
│             │  High Memory (RAM)      │ │ 64KB
│  0x00000000 ├─────────────────────────┤ │
│             │  Conventional Memory    │ │ 640KB
│  0x00000000 └─────────────────────────┘ │
│                                         │
└─────────────────────────────────────────┘

Implementation:
├─ MemoryRegion("RAM")
│  ├─ 0x00000000: Conventional + Extended
│  └─ Type: RAM (allocated at startup)
│
├─ MemoryRegion("PCI-IO")
│  ├─ 0xC0000000-0xFFFFFFFF: PCI address space
│  └─ Type: I/O (device-specific handlers)
│
└─ MemoryRegion("BIOS")
   ├─ 0xFFFE0000: BIOS ROM
   └─ Type: ROM (read-only)
```

### Region Lookup

```
Guest accesses address GPA:
    1. Lookup MemoryRegion for GPA
       └─> Binary search or hash table
    
    2. Translate to region-relative address
       └─> offset = GPA - region_base
    
    3. Check access type (read/write)
       └─> Verify permissions
    
    4. Call region handler (if I/O)
       └─> ops->read() or ops->write()
    
    5. Return data or complete write
       └─> Update TLB if appropriate
```

## IOMMU Support

An IOMMU (Input/Output Memory Management Unit) handles device memory access.

### Device DMA Translation

```
Without IOMMU:
    Device Memory Access (DMA):
        Device Virtual Address (DVA)
            │
            ├─ Direct memory access
            └─> Host Physical Address (HPA)
        
    Problem: Device can access any memory

With IOMMU:
    Device Memory Access (DMA):
        Device Virtual Address (DVA)
            │
            ├─ IOMMU Translation
            └─> Device Physical Address (DPA)
            │
            └─> Guest Physical Address (GPA)
            │
            └─> Host Virtual Address (HVA)
        
    Benefit: Device memory access is restricted to VM's memory
```

### IOMMU Implementation in QEMU

```c
struct IOMMUMemoryRegion {
    MemoryRegion iommu_mr;
    
    const IOMMUMemoryRegionClass *iommu_class;
    
    void (*translate)(IOMMUMemoryRegion *iommu, Address addr,
                      IOMMUTLBEntry *tlb_entry, void *opaque);
};
```

## Memory Optimization

### Copy-on-Write (CoW)

Used for snapshots and live migration:

```
Original VM:
    ┌─────────────────┐
    │   Page (4KB)    │ (Private)
    └─────────────────┘
         ▲
         │ (Single reference)

Snapshot/Migrate:
    ┌─────────────────┐
    │   Page (4KB)    │ (Shared)
    └─────────────────┘
       ▲     ▲
       │     │
    VM1  VM2 (or Snapshot)

On Write:
    1. Page fault (write protection)
    2. Allocate new page
    3. Copy data
    4. Update reference
```

### Memory Deduplication

Reduces memory footprint when identical pages exist:

```
Initial State (2 identical pages):
    ┌────────────┐  ┌────────────┐
    │ "QEMU OS"  │  │ "QEMU OS"  │
    └────────────┘  └────────────┘
       Page 1        Page 2

After Deduplication:
    ┌────────────┐
    │ "QEMU OS"  │ ◄── Shared
    └────────────┘
       ▲           ▲
       │           │
    Page 1    Page 2 (read-only link)
    
Memory Saved: 4KB per identical page
```

### Transparent Hugepages

Uses larger page sizes (2MB, 1GB) to reduce TLB misses:

```
Standard 4KB Pages:
    ├─ Page 1: 0x00000000-0x00000FFF
    ├─ Page 2: 0x00001000-0x00001FFF
    ├─ Page 3: 0x00002000-0x00002FFF
    └─ ... (1M pages for 4GB memory)
    └─ TLB entries needed: 1M+

Huge Pages (2MB):
    ├─ Huge Page 1: 0x00000000-0x001FFFFF
    ├─ Huge Page 2: 0x00200000-0x003FFFFF
    └─ ... (2000 pages for 4GB memory)
    └─ TLB entries needed: 2000
    
Benefits:
    - Fewer TLB misses
    - Lower TLB pressure
    - Faster address translation
    - ~30-50% performance improvement
```

---

*See related documents: [Core Emulation Systems](02-core-emulation.md), [I/O and Interrupt Handling](09-io-interrupts.md)*
