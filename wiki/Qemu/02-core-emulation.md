# Core Emulation Systems

## Table of Contents
1. [CPU Emulation Overview](#cpu-emulation-overview)
2. [TCG (Tiny Code Generator)](#tcg-tiny-code-generator)
3. [Instruction Translation](#instruction-translation)
4. [KVM Acceleration](#kvm-acceleration)
5. [Other Accelerators](#other-accelerators)
6. [CPU State Management](#cpu-state-management)

## CPU Emulation Overview

QEMU supports CPU emulation through multiple backends:

- **TCG**: Software-based JIT compilation (portable, universal)
- **KVM**: Hardware virtual machine extensions (fast, Linux only)
- **HVF**: macOS Hypervisor Framework
- **HCX, WHPX**: Windows accelerators
- **Xen**: Xen hypervisor integration

```
┌──────────────────────────────────────┐
│         QEMU CPU Interface            │
│   (cpu_exec, cpu_interrupt, etc.)     │
└──────────────────┬───────────────────┘
                   │
        ┌──────────┼──────────┐
        │          │          │
        ▼          ▼          ▼
     ┌──────┐  ┌──────┐  ┌────────┐
     │ TCG  │  │ KVM  │  │  HVF   │
     │(JIT) │  │ (HW) │  │ (macOS)│
     └──────┘  └──────┘  └────────┘
        │          │          │
        └──────────┼──────────┘
                   │
        ┌──────────▼──────────┐
        │   Host CPU/Kernel    │
        └──────────────────────┘
```

## TCG (Tiny Code Generator)

TCG is QEMU's portable CPU emulation engine. It translates guest instructions to host instructions using Just-In-Time (JIT) compilation.

### Architecture

```
Guest Binary Code
        │
        ▼
┌──────────────────┐
│ TCG Translator   │
│ (per-arch)       │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ TCG IR (Ops)     │
│ (Intermediate    │
│  Representation) │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ TCG Backend      │
│ (per-host CPU)   │
└────────┬─────────┘
         │
         ▼
    Host Code
   (x86/ARM/etc)
```

### TCG Translation Process

```
1. Fetch guest instructions
   └─> Determine instruction boundaries
   └─> Decode instruction semantics

2. Generate TCG Intermediate Representation (IR)
   └─> Convert guest instruction to TCG ops
   └─> Handle CPU state updates
   └─> Manage condition flags

3. Optimize TCG IR
   └─> Constant folding
   └─> Dead code elimination
   └─> Instruction reordering

4. Generate Host Code
   └─> Select host CPU instructions
   └─> Allocate registers
   └─> Manage CPU state in memory

5. Cache Translation Block
   └─> Store in code buffer
   └─> Link to other blocks
   └─> Enable code reuse
```

### Translation Block (TB)

A translation block is a sequence of guest instructions ending in:
- Branch/jump
- Interrupt check point
- TB size limit

```c
struct TranslationBlock {
    target_ulong pc;           // Guest program counter
    target_ulong cs_base;      // Code segment base (x86)
    uint32_t flags;            // CPU state flags
    uint32_t size;             // Block size in bytes
    void *tc_ptr;              // Pointer to generated code
    struct TranslationBlock **jmp_list_next;
    unsigned int icount;       // Number of instructions
};
```

### Code Cache

```
Host Memory Code Cache
┌────────────────────────────┐
│ [TB1] [TB2] [TB3] ... [TBn]│
└────────────────────────────┘
    ▲       ▲       ▲
    │       │       │
Linked via jump targets
```

**Cache Management:**
- Circular buffer with overflow handling
- Flush on context switch
- Invalidate on TB collision
- Performance: Balances cache size vs. hit rate

### TCG Optimization Passes

1. **Constant Propagation**: Pre-compute values at translation time
2. **Dead Code Elimination**: Remove unused operations
3. **Register Allocation**: Optimize for host CPU registers
4. **Loop Unrolling**: Inline repeated blocks

### Multi-threaded TCG

```
Main Thread              VCPU Thread 1      VCPU Thread 2
│                        │                  │
├─ Event Loop            ├─ CPU Loop       ├─ CPU Loop
│  ├─ I/O                │  │              │  │
│  └─ Sync Points        │  └─ TB Exec     │  └─ TB Exec
│                        │                  │
└─────┬────────────────────────────────────┘
      │
      ▼
  Shared TB Cache
  & Memory
```

## Instruction Translation

### Generic Translation Flow

```
For each guest instruction:
    1. Decode instruction
    2. Extract operand information
    3. Generate TCG operations for:
       ├─ Data movement
       ├─ Arithmetic/Logic operations
       ├─ Memory access
       ├─ Branch/Jump
       └─ Flag updates
    4. Handle CPU state updates
```

### Example: x86 ADD instruction

```
Guest Instruction: add eax, ebx

TCG Operations Generated:
  1. tcg_gen_add_i32(eax, eax, ebx)     // Perform addition
  2. tcg_gen_setcond_i32(cond, flags)   // Update condition flags
  3. tcg_gen_mov_i32(cpu_eax, eax)      // Store result to CPU state

Host Code Generated (x86_64):
  mov %r8d, (%r9)         # Load eax
  add %r10d, %r8d         # Add ebx
  mov %r8d, (%r9)         # Store eax
  [flags update logic]
```

## KVM Acceleration

KVM provides hardware-based virtualization using CPU extensions (Intel VT-x, AMD SVM).

### KVM Architecture

```
Guest User Space       QEMU User Space         Host Kernel
     │                      │                        │
     ├─ Guest Code           ├─ Device I/O           ├─ KVM Module
     │                       │  Emulation            │
     │                       │  Monitor              ├─ Hypervisor
     │                       ├─ ioctl(KVM_RUN)      │
     │                       │  ◄──────────────────►├─ Hardware CPU
     │                       │  (VM Enter/Exit)     │  (VT-x/SVM)
     │                       │                      │
     └───────────────────────┴──────────────────────┘
```

### KVM Execution Flow

```
1. QEMU calls ioctl(KVM_RUN)
   │
   └─> Enter guest mode
       ├─ Load guest CPU state (registers)
       ├─ Set up memory mappings
       └─> Execute guest code on real CPU

2. Guest Execution
   │
   └─> Runs on bare metal until:
       ├─ I/O instruction detected
       ├─ Page fault (EPT/NPT violation)
       ├─ Interrupt/Exception
       └─> Hardware raises VM Exit

3. VM Exit (return to QEMU)
   │
   └─> QEMU handles:
       ├─ Save guest state
       ├─ Emulate I/O operation
       ├─ Update TLB/memory
       └─> Resume guest (back to step 1)
```

### Memory Management in KVM

```
Guest Physical Address Space (GPA)
        │
        ▼
┌──────────────────┐
│ KVM Memory Slot  │ (shadow page table)
│ (EPT/NPT)        │
└────────┬─────────┘
         │
         ▼
Host Virtual Address Space (HVA/GVA)
        │
        ▼
┌──────────────────┐
│ Linux Page Table │ (via host kernel)
└────────┬─────────┘
         │
         ▼
Host Physical Address Space (HPA)
```

## Other Accelerators

### HVF (Hypervisor Framework - macOS)

Similar to KVM but uses Apple's Hypervisor Framework:
- Manages virtual CPU execution
- MMU handling via EPT-like mechanism
- Interrupt injection support

### HCX (Intel - Windows/Android)

Hardware acceleration for x86:
- HAXM (Hardware Accelerated Execution Manager)
- VT-x extensions on Windows

### WHPX (Windows Hypervisor Platform)

Microsoft's hypervisor interface:
- Similar to KVM concept
- Supports x86 virtualization

### Xen Integration

QEMU can run as a domain in Xen:
- Xen handles hardware access
- QEMU emulates user-mode devices
- No direct hardware access

## CPU State Management

### CPU State Structure

```c
struct CPUX86State {
    // General purpose registers
    target_ulong regs[8];           // RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI
    
    // Segment registers
    SegmentCache segs[6];           // CS, DS, SS, ES, FS, GS
    
    // Control registers
    target_ulong cr[5];             // CR0, CR2, CR3, CR4
    
    // Debug registers
    target_ulong dr[8];
    
    // Floating point & SSE
    struct {
        uint8_t data[8*16];         // XMM/YMM registers
    } xmm_regs[16];
    
    // Flags and condition codes
    int32_t cc_op;                  // Condition code operation
    target_ulong cc_src, cc_dst;    // Condition code operands
    
    // Control flow
    target_ulong eip;               // Instruction pointer
    target_ulong esp, ebp;          // Stack pointers
    
    // Memory management
    target_ulong base;              // Segment base
    uint32_t limit;                 // Segment limit
};
```

### Context Switches

```
VCPU Thread 1 Running:
  ├─ Load CPU state from cpu->env
  ├─ Execute TB instructions
  └─ Save CPU state on context switch

Synchronization Point:
  ├─ Global BQL (Big Kernel Lock)
  ├─ Flush TLB if needed
  └─ Allow device I/O processing

VCPU Thread 2 Running:
  ├─ Load CPU state from cpu->env
  ├─ Execute TB instructions
  └─ Save CPU state on context switch
```

### CPU Interrupt Handling

```
Interrupt Signaling:
  1. Device/External source signals interrupt
  2. qemu_cpu_kick() called for target VCPU
  3. VCPU checks cpu_interrupt_request flag

Interrupt Processing:
  1. At TB boundary or periodically
  2. Check interrupt type and priority
  3. Jump to interrupt handler
  4. Save CPU state
  5. Load interrupt vector/handler address
  6. Resume execution
```

## Performance Characteristics

### TCG Performance

- **Speed**: 5-50x slower than native (depends on workload)
- **Startup**: Longer due to translation
- **Throughput**: Improves with translation caching
- **Memory**: ~5-10MB per translation block cache

### KVM Performance

- **Speed**: Near-native (2-5% overhead typical)
- **Startup**: Immediate, no translation
- **Limitations**: Linux-only, requires VT-x/SVM
- **Memory**: Lower memory overhead

---

*See related documents: [Memory Management](03-memory-management.md), [Advanced Topics](10-advanced-topics.md)*
