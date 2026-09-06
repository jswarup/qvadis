# QemuRuntime Library - Detailed Implementation Plan

**Project**: Qvadis - QEMU Runtime Library  
**Objective**: Embed QEMU into C++ host applications as a reusable, encapsulated library  
**Target Libraries**: `libqemuruntime.so` (Linux) | `qemuruntime.dll` (Windows)  
**QEMU Pinned Version**: ../qemu

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Phase 1-2: Foundation & Build Setup](#phase-1-2-foundation--build-setup)
3. [Phase 3-4: Core C API & Event Loop](#phase-3-4-core-c-api--event-loop-integration)
4. [Phase 5: Machine Management](#phase-5-machine-management-api)
5. [Phase 6: Storage & Disk I/O](#phase-6-storage--disk-io)
6. [Phase 7: Console & Streaming](#phase-7-console--character-device-streaming)
7. [Phase 8: Event Callbacks](#phase-8-event-callbacks--notifications)
8. [Phase 9: Hardware Acceleration](#phase-9-hardware-acceleration)
9. [Phase 10: QMP Integration](#phase-10-qmpqapi-integration)
10. [Testing Strategy](#testing-strategy)
11. [Repository Structure](#repository-structure)

---

## Architecture Overview

```
┌────────────────────────────────────────────┐
│     Host Application (CMake-based)         │  Uses QemuRuntime library
├────────────────────────────────────────────┤
│  C++ API Layer (QemuRuntime namespace)     │  Clean, type-safe interface
├────────────────────────────────────────────┤
│  C ABI Boundary (public C interface)       │  Stable, compiler-agnostic
├────────────────────────────────────────────┤
│  C API Implementation Layer                │  Bridges C API to QEMU
├────────────────────────────────────────────┤
│  QEMU Integration Layer                    │  Event loop, machine mgmt
│  - Machine wrappers                        │  - Loop integration
│  - Device management                       │  - Thread synchronization
│  - I/O handling                            │
├────────────────────────────────────────────┤
│  Unmodified QEMU Core (stable-11.0)        │  No modifications to QEMU
└────────────────────────────────────────────┘
```

### Key Design Patterns

- **Opaque Pointers**: All QEMU structures passed as `void*` or typed opaque handles
- **Resource Ownership**: Explicit ownership transfer in API (creator destroys)
- **Error Handling**: Status codes (enums) returned from C API; exceptions in C++ layer
- **Thread Safety**: Mutex-protected access to QEMU event loop

---

## Phase 1-2: Foundation & Build Setup

### Objectives
- Verify QEMU builds on both Linux and Windows
- Document QEMU initialization flow
- Establish build infrastructure
- Create repository structure

### Deliverables

#### Task 1.1: Repository Setup
- [ ] Create qvadis repository structure:
  ```
  qvadis/
  ├── CMakeLists.txt (top-level)
  ├── README.md
  ├── docs/
  │   ├── ARCHITECTURE.md
  │   ├── QEMU_INTERNALS.md
  │   └── BUILD.md
  ├── src/
  │   ├── c_api/          (Phase 3)
  │   ├── cpp_wrapper/    (Phase 3)
  │   └── integration/    (Phase 4)
  ├── tests/
  │   └── phase_1_build_tests/
  ├── third_party/
  │   └── qemu/           (submodule or clone)
  ├── cmake/
  │   └── FindQEMU.cmake
  └── build/
      ├── linux/
      └── windows/ 

#### Task 1.2: Build QEMU (Baseline)

**Linux Build**:
- [ ] Install dependencies: `build-essential`, `pkg-config`, `python3`, `ninja-build`, `libglib2.0-dev`, `libpixman-1-dev`
- [ ] Configure QEMU:
  ```bash
  cd third_party/qemu
  ./configure --prefix=/opt/qemu-stable-11.0 \
    --target-list=x86_64-softmmu \
    --enable-debug \
    --disable-docs
  ```
- [ ] Build and install:
  ```bash
  make -j$(nproc)
  make install
  ```

**Windows Build** (MSVC or MinGW):
- [ ] Set up MSVC build environment or MinGW toolchain
- [ ] Build using QEMU's meson configuration:
  ```bash
  cd third_party/qemu
  meson setup build --prefix=C:\Work\Oriole\qemu -Dtarget_list=x86_64-softmmu
  meson compile -C build
  meson install -C build
  ```

#### Task 1.3: QEMU Internals Documentation

Document the following in `docs/QEMU_INTERNALS.md`:

- [ ] **Startup Flow**:
  - Entry point: `main()` in `softmmu/main.c`
  - Machine initialization: `machine_class->init()` callback
  - Device initialization flow
  - Event loop startup in `qemu_main_loop()`

- [ ] **Key Internal Structures** (for reference, not exposed):
  - `MachineState` - VM state container
  - `CPUState` - Per-CPU execution context
  - `AddressSpace` - Memory management
  - `ChardevBackend` - Character device I/O
  - `BlockBackend` - Storage abstraction

- [ ] **Event Loop Architecture**:
  - `aio_context_t` - Async I/O context
  - Main loop integration points
  - File descriptor polling mechanism
  - Timer/timeout handling

- [ ] **Critical Functions for Integration**:
  - `qemu_init()` vs `qemu_main()`
  - Event loop entry: `qemu_main_loop_wait()`, `aio_poll()`
  - Machine setup: `qemu_machine_init_done_notifiers`
  - Shutdown: `qemu_system_shutdown_request()`

#### Task 1.4: Test QEMU Functionality

- [x] Create test harness in `tests/phase_1_build_tests/`:
  - Verify QEMU executable runs
  - Boot minimal Linux kernel (or OVMF BIOS)
  - Verify x86_64 Q35 machine type works
  - Verify TCG execution

- [x] Document build configuration for both platforms

### Success Criteria
- ✅ QEMU compiles without errors on Linux and Windows
- ✅ Unmodified QEMU runs a simple VM
- ✅ Startup flow documented and understood
- ✅ Repository structure established

---

## Phase 3-4: Core C API & Event Loop Integration

### Objectives
- Define the public C ABI boundary
- Create minimal C API
- Integrate QEMU event loop with host threads
- Build proof-of-concept C wrapper

### Deliverables

#### Task 3.1: Design C API Header

Create `src/c_api/qemu_runtime.h`:

```c
#ifndef QEMU_RUNTIME_H
#define QEMU_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle types */
typedef void* qemu_runtime_t;
typedef void* qemu_machine_t;
typedef void* qemu_console_t;

/* Status codes */
typedef enum {
    QEMU_OK = 0,
    QEMU_ERR_INVALID_ARG = 1,
    QEMU_ERR_ALREADY_RUNNING = 2,
    QEMU_ERR_NOT_RUNNING = 3,
    QEMU_ERR_INITIALIZATION = 4,
    QEMU_ERR_OUT_OF_MEMORY = 5,
    QEMU_ERR_UNKNOWN = 255
} qemu_status_t;

/* Configuration structure */
typedef struct {
    const char* machine_type;      /* e.g., "q35" */
    const char* cpu_model;         /* e.g., "host" or "qemu64" */
    uint64_t memory_mb;            /* RAM in megabytes */
    const char* kernel_path;       /* Optional kernel image */
    const char* rootfs_path;       /* Optional root filesystem */
} qemu_config_t;

/* === Runtime Lifecycle === */

/**
 * Create a new QEMU runtime instance
 * @return Opaque handle to runtime, NULL on failure
 */
qemu_runtime_t qemu_runtime_create(void);

/**
 * Initialize runtime with configuration
 * @return Status code
 */
qemu_status_t qemu_runtime_init(qemu_runtime_t runtime, const qemu_config_t* config);

/**
 * Start emulation
 * @return Status code
 */
qemu_status_t qemu_runtime_start(qemu_runtime_t runtime);

/**
 * Pause emulation (non-blocking)
 * @return Status code
 */
qemu_status_t qemu_runtime_pause(qemu_runtime_t runtime);

/**
 * Resume emulation after pause
 * @return Status code
 */
qemu_status_t qemu_runtime_resume(qemu_runtime_t runtime);

/**
 * Stop emulation
 * @return Status code
 */
qemu_status_t qemu_runtime_stop(qemu_runtime_t runtime);

/**
 * Destroy runtime and free resources
 */
void qemu_runtime_destroy(qemu_runtime_t runtime);

/* === Event Loop Integration === */

/**
 * Run QEMU event loop for specified milliseconds
 * Non-blocking if timeout=0
 * @return Status code
 */
qemu_status_t qemu_runtime_pump_events(qemu_runtime_t runtime, uint32_t timeout_ms);

/**
 * Query if emulation is running
 */
int qemu_runtime_is_running(qemu_runtime_t runtime);

/**
 * Query if emulation is paused
 */
int qemu_runtime_is_paused(qemu_runtime_t runtime);

/* === Version Info === */

const char* qemu_runtime_version(void);

#ifdef __cplusplus
}
#endif

#endif /* QEMU_RUNTIME_H */
```

#### Task 3.2: Thread-Safe QEMU Context Wrapper

Create `src/integration/qemu_context.h/cpp`:

```cpp
// qemu_context.h
#pragma once

#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace qvadis {

class QemuContext {
public:
    QemuContext();
    ~QemuContext();

    // Initialize with QEMU machine configuration
    bool Initialize(const char* machine_type, uint64_t memory_mb);

    // Lifecycle control
    bool Start();
    bool Pause();
    bool Resume();
    bool Stop();

    // Event loop pump - safely integrates QEMU event loop with host
    bool PumpEvents(uint32_t timeout_ms);

    // State queries
    bool IsRunning() const;
    bool IsPaused() const;

private:
    // QEMU context mutex - protects all QEMU state
    mutable std::mutex qemu_mutex_;

    // Condition variable for pause/resume synchronization
    std::condition_variable state_cv_;

    // Runtime state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};

    // Opaque QEMU context (to be filled with actual pointers)
    void* qemu_state_;

    // Helper: Safely execute QEMU operations under lock
    template<typename Func>
    bool WithQemuLock(Func&& fn);
};

} // namespace qvadis
```

#### Task 3.3: C API Implementation

Create `src/c_api/qemu_runtime.c`:

```c
#include "qemu_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Wrapper for C++ QemuContext */
typedef struct {
    void* cpp_context;  /* QemuContext* */
    pthread_mutex_t lock;
} qemu_runtime_impl_t;

qemu_runtime_t qemu_runtime_create(void) {
    qemu_runtime_impl_t* rt = malloc(sizeof(*rt));
    if (!rt) return NULL;

    pthread_mutex_init(&rt->lock, NULL);
    
    /* Create C++ QemuContext via extern "C" factory function */
    rt->cpp_context = qvadis_cpp_context_create();
    
    if (!rt->cpp_context) {
        free(rt);
        return NULL;
    }
    
    return rt;
}

qemu_status_t qemu_runtime_init(qemu_runtime_t runtime, const qemu_config_t* config) {
    if (!runtime || !config) return QEMU_ERR_INVALID_ARG;
    
    qemu_runtime_impl_t* rt = (qemu_runtime_impl_t*)runtime;
    pthread_mutex_lock(&rt->lock);
    
    qemu_status_t result = qvadis_cpp_context_init(
        rt->cpp_context,
        config->machine_type,
        config->memory_mb
    );
    
    pthread_mutex_unlock(&rt->lock);
    return result;
}

qemu_status_t qemu_runtime_start(qemu_runtime_t runtime) {
    if (!runtime) return QEMU_ERR_INVALID_ARG;
    
    qemu_runtime_impl_t* rt = (qemu_runtime_impl_t*)runtime;
    pthread_mutex_lock(&rt->lock);
    qemu_status_t result = qvadis_cpp_context_start(rt->cpp_context);
    pthread_mutex_unlock(&rt->lock);
    return result;
}

qemu_status_t qemu_runtime_pump_events(qemu_runtime_t runtime, uint32_t timeout_ms) {
    if (!runtime) return QEMU_ERR_INVALID_ARG;
    
    qemu_runtime_impl_t* rt = (qemu_runtime_impl_t*)runtime;
    pthread_mutex_lock(&rt->lock);
    qemu_status_t result = qvadis_cpp_context_pump_events(rt->cpp_context, timeout_ms);
    pthread_mutex_unlock(&rt->lock);
    return result;
}

/* ... additional functions ... */

void qemu_runtime_destroy(qemu_runtime_t runtime) {
    if (!runtime) return;
    
    qemu_runtime_impl_t* rt = (qemu_runtime_impl_t*)runtime;
    qvadis_cpp_context_destroy(rt->cpp_context);
    pthread_mutex_destroy(&rt->lock);
    free(rt);
}
```

#### Task 3.4: QEMU Integration Hooks

Create `src/integration/qemu_init_hooks.cpp`:

- [x] **Patch or wrap QEMU main entry**:
  - Instead of calling `main()`, call `qemu_init()` and manage event loop manually
  - Hook into `qemu_main_loop_wait()` for non-blocking integration

- [x] **Event loop integration**:
  ```cpp
  bool QemuContext::PumpEvents(uint32_t timeout_ms) {
      std::unique_lock<std::mutex> lock(qemu_mutex_);
      
      // Call QEMU's non-blocking event pump
      // This processes pending I/O, timers, and signals without blocking
      aio_poll(qemu_aio_context, timeout_ms);
      
      return true;
  }
  ```

#### Task 3.5: Build CMakeLists.txt

Create top-level and modular CMake files:

- [x] `CMakeLists.txt` - Top-level project definition
- [x] `src/CMakeLists.txt` - C API and integration code
- [x] `src/c_api/CMakeLists.txt` - C API library target
- [x] `src/cpp_wrapper/CMakeLists.txt` - C++ wrapper library
- [x] Link against QEMU's build output

#### Task 3.6: Unit Tests

Create `tests/phase_3_4_tests/`:

- [x] Test C API creation/destruction
- [x] Test initialization with valid config
- [x] Test lifecycle transitions (create → init → start → stop → destroy)
- [x] Test event loop pumping
- [x] Test thread-safety (concurrent access to runtime)

### Success Criteria
- ✅ C API header complete and documented
- ✅ C++ QemuContext wrapper functional
- ✅ Thread-safe mutex protection in place
- ✅ QEMU event loop integrates with host application
- ✅ Basic lifecycle operations work (start, pause, resume, stop)
- ✅ All tests pass on Linux and Windows

---

## Phase 5: Machine Management API

### Objectives
- Expose machine creation and configuration
- Hide `MachineState` behind opaque handle
- Support machine lifecycle

### Deliverables

#### Task 5.1: Machine C API Extension

Add to `qemu_runtime.h`:

```c
/* Machine management */
qemu_status_t qemu_runtime_create_machine(
    qemu_runtime_t runtime,
    const char* machine_type,
    qemu_machine_t* out_machine
);

qemu_status_t qemu_machine_configure_cpu(
    qemu_machine_t machine,
    const char* cpu_model,
    uint32_t num_cpus
);

qemu_status_t qemu_machine_configure_memory(
    qemu_machine_t machine,
    uint64_t size_mb
);

qemu_status_t qemu_machine_destroy(qemu_machine_t machine);
```

#### Task 5.2: Machine Wrapper Implementation

Create `src/integration/qemu_machine.cpp`:

- [x] Opaque `QemuMachine` class wrapping `MachineState`
- [x] CPU and memory configuration methods
- [x] Machine state tracking

#### Task 5.3: C++ API Wrapper

Create `src/cpp_wrapper/QemuMachine.hpp`:

```cpp
namespace qvadis {

class QemuMachine {
public:
    explicit QemuMachine(qemu_machine_t handle);
    ~QemuMachine();

    // Delete copy, allow move
    QemuMachine(const QemuMachine&) = delete;
    QemuMachine& operator=(const QemuMachine&) = delete;
    QemuMachine(QemuMachine&&) noexcept;

    void ConfigureCPU(const std::string& model, uint32_t count);
    void ConfigureMemory(uint64_t size_mb);

private:
    qemu_machine_t handle_;
};

} // namespace qvadis
```

### Success Criteria
- ✅ Machine creation/destruction works
- ✅ CPU and memory configuration functional
- ✅ RAII-based C++ wrapper handles resource cleanup

---

## Phase 6: Storage & Disk I/O

### Objectives
- Expose block device management
- Support disk image attachment
- Hide `BlockBackend` and `BlockState` details

### Deliverables

#### Task 6.1: Storage C API

Add to `qemu_runtime.h`:

```c
typedef void* qemu_disk_t;

typedef enum {
    QEMU_DISK_FORMAT_RAW = 0,
    QEMU_DISK_FORMAT_QCOW2 = 1,
} qemu_disk_format_t;

qemu_status_t qemu_machine_attach_disk(
    qemu_machine_t machine,
    const char* path,
    qemu_disk_format_t format,
    const char* device_id,
    qemu_disk_t* out_disk
);

qemu_status_t qemu_machine_detach_disk(
    qemu_machine_t machine,
    qemu_disk_t disk
);

qemu_status_t qemu_disk_read(
    qemu_disk_t disk,
    uint64_t offset,
    uint8_t* buffer,
    size_t length,
    size_t* out_bytes_read
);

qemu_status_t qemu_disk_write(
    qemu_disk_t disk,
    uint64_t offset,
    const uint8_t* buffer,
    size_t length,
    size_t* out_bytes_written
);
```

#### Task 6.2: Block Device Wrapper

Create `src/integration/qemu_disk.cpp`:

- [x] Wrap `BlockBackend` in opaque handle
- [x] Implement disk read/write operations
- [x] Handle format detection and conversion

#### Task 6.3: Integration Tests

Create `tests/phase_6_tests/`:

- [x] Create and attach virtual disk
- [x] Read/write operations
- [x] Disk detachment
- [x] Multiple disk support

### Success Criteria
- ✅ Disk attachment/detachment works
- ✅ Read/write operations functional
- ✅ Multiple disk configurations supported

---

## Phase 7: Console & Character Device Streaming

### Objectives
- Stream serial console output to host
- Support bidirectional console I/O
- Hide character device details

### Deliverables

#### Task 7.1: Console C API

Add to `qemu_runtime.h`:

```c
typedef void (*qemu_console_read_callback_t)(
    const uint8_t* data,
    size_t length,
    void* user_data
);

qemu_status_t qemu_machine_attach_console(
    qemu_machine_t machine,
    const char* device_id,
    qemu_console_t* out_console
);

qemu_status_t qemu_console_set_input_callback(
    qemu_console_t console,
    qemu_console_read_callback_t callback,
    void* user_data
);

qemu_status_t qemu_console_write(
    qemu_console_t console,
    const uint8_t* data,
    size_t length
);
```

#### Task 7.2: Console Backend Implementation

Create `src/integration/qemu_console.cpp`:

- [ ] Wrap QEMU `ChardevBackend` as console
- [ ] Implement output buffering
- [ ] Callback-based input delivery

#### Task 7.3: Console Tests

Create `tests/phase_7_tests/`:

- [ ] Console output streaming
- [ ] Console input injection
- [ ] Bidirectional I/O

### Success Criteria
- ✅ Console output readable from host
- ✅ Console input writable from host
- ✅ Streaming works during emulation

---

## Phase 8: Event Callbacks & Notifications

### Objectives
- Notify host of machine state changes
- Expose I/O completion callbacks
- Support event subscription model

### Deliverables

#### Task 8.1: Event System C API

Add to `qemu_runtime.h`:

```c
typedef enum {
    QEMU_EVENT_MACHINE_STARTED = 1,
    QEMU_EVENT_MACHINE_PAUSED = 2,
    QEMU_EVENT_MACHINE_STOPPED = 3,
    QEMU_EVENT_DISK_READ_COMPLETE = 4,
    QEMU_EVENT_DISK_WRITE_COMPLETE = 5,
    QEMU_EVENT_ERROR = 255
} qemu_event_type_t;

typedef struct {
    qemu_event_type_t type;
    uint64_t timestamp_ns;
    void* context;
    const char* message;
} qemu_event_t;

typedef void (*qemu_event_callback_t)(const qemu_event_t* event, void* user_data);

qemu_status_t qemu_runtime_subscribe_event(
    qemu_runtime_t runtime,
    qemu_event_type_t event_type,
    qemu_event_callback_t callback,
    void* user_data
);

qemu_status_t qemu_runtime_unsubscribe_event(
    qemu_runtime_t runtime,
    qemu_event_type_t event_type,
    qemu_event_callback_t callback
);
```

#### Task 8.2: Event Dispatcher Implementation

Create `src/integration/qemu_event_dispatcher.cpp`:

- [ ] Thread-safe event queue
- [ ] Callback invocation during `qemu_runtime_pump_events()`
- [ ] Event filtering by type

#### Task 8.3: Event Tests

Create `tests/phase_8_tests/`:

- [ ] Subscribe/unsubscribe operations
- [ ] Event delivery correctness
- [ ] Callback invocation timing
- [ ] Multiple subscribers

### Success Criteria
- ✅ Event subscription works
- ✅ Events delivered reliably
- ✅ Thread-safe callback dispatch

---

## Phase 9: Hardware Acceleration

### Objectives
- Add KVM support (Linux)
- Add WHPX support (Windows)
- Automatic detection and fallback to TCG

### Deliverables

#### Task 9.1: Acceleration Detection

Create `src/integration/qemu_acceleration.cpp`:

- [ ] Detect KVM availability (Linux)
- [ ] Detect WHPX availability (Windows)
- [ ] Graceful fallback to TCG

#### Task 9.2: Acceleration C API

Add to `qemu_runtime.h`:

```c
typedef enum {
    QEMU_ACCEL_TCG = 0,    /* Software emulation (always available) */
    QEMU_ACCEL_KVM = 1,    /* Linux KVM */
    QEMU_ACCEL_WHPX = 2,   /* Windows Hyper-V */
} qemu_accelerator_t;

qemu_accelerator_t qemu_machine_get_accelerator(qemu_machine_t machine);

qemu_status_t qemu_runtime_set_preferred_accelerator(
    qemu_runtime_t runtime,
    qemu_accelerator_t accel
);
```

#### Task 9.3: Acceleration Tests

Create `tests/phase_9_tests/`:

- [ ] Detect available accelerators
- [ ] Boot with KVM (if available)
- [ ] Boot with WHPX (if available)
- [ ] Fallback to TCG
- [ ] Performance benchmarking

### Success Criteria
- ✅ KVM works on Linux
- ✅ WHPX works on Windows
- ✅ Graceful TCG fallback
- ✅ 10-50x speedup with hardware acceleration

---

## Phase 10: QMP/QAPI Integration

### Objectives
- Expose QEMU Management Protocol
- High-level command execution
- Status and metrics queries

### Deliverables

#### Task 10.1: QMP C API

Add to `qemu_runtime.h`:

```c
typedef struct {
    char* response;  /* JSON response from QMP */
    size_t response_len;
} qemu_qmp_response_t;

void qemu_qmp_response_free(qemu_qmp_response_t* resp);

qemu_status_t qemu_runtime_execute_qmp_command(
    qemu_runtime_t runtime,
    const char* command_json,
    qemu_qmp_response_t* out_response
);
```

#### Task 10.2: QMP Command Wrapper Implementation

Create `src/integration/qemu_qmp.cpp`:

- [ ] Connect to QEMU QMP socket
- [ ] Command serialization/deserialization
- [ ] Response parsing

#### Task 10.3: Common QMP Commands

Implement convenience functions for:

```c
qemu_status_t qemu_machine_query_status(
    qemu_machine_t machine,
    char** out_status_json
);

qemu_status_t qemu_machine_query_stats(
    qemu_machine_t machine,
    char** out_stats_json
);
```

#### Task 10.4: QMP Tests

Create `tests/phase_10_tests/`:

- [ ] QMP command execution
- [ ] Status queries
- [ ] Advanced VM control via QMP
- [ ] JSON response parsing

### Success Criteria
- ✅ QMP commands executable
- ✅ Status and stats queries work
- ✅ JSON responses properly parsed

---

## Testing Strategy

### Test Structure

```
tests/
├── phase_1_build_tests/
│   └── test_qemu_baseline.cpp
├── phase_3_4_tests/
│   ├── test_c_api_lifecycle.c
│   ├── test_thread_safety.cpp
│   └── test_event_loop_integration.cpp
├── phase_5_tests/
│   └── test_machine_management.cpp
├── phase_6_tests/
│   └── test_disk_io.cpp
├── phase_7_tests/
│   └── test_console_streaming.cpp
├── phase_8_tests/
│   └── test_event_callbacks.cpp
├── phase_9_tests/
│   └── test_acceleration.cpp
├── phase_10_tests/
│   └── test_qmp_integration.cpp
├── integration_tests/
│   └── test_full_workflow.cpp
└── CMakeLists.txt
```

### Test Categories

1. **Unit Tests**: Individual API functions
2. **Integration Tests**: Multi-layer workflows
3. **Cross-Platform Tests**: Linux + Windows validation
4. **Performance Tests**: Acceleration benchmarks
5. **Stress Tests**: Long-running VMs, memory stability

### CI/CD Integration

- [ ] GitHub Actions workflow for Linux/Windows builds
- [ ] Automated test runs on commit
- [ ] Performance regression detection

---

## Repository Structure

### Final Layout

```
qvadis/
├── .github/
│   └── workflows/
│       ├── build-linux.yml
│       ├── build-windows.yml
│       └── tests.yml
├── cmake/
│   ├── FindQEMU.cmake
│   └── QemuRuntimeConfig.cmake.in
├── docs/
│   ├── ARCHITECTURE.md
│   ├── QEMU_INTERNALS.md
│   ├── BUILD.md
│   ├── API_REFERENCE.md
│   └── EXAMPLES.md
├── src/
│   ├── CMakeLists.txt
│   ├── c_api/
│   │   ├── CMakeLists.txt
│   │   ├── qemu_runtime.h
│   │   └── qemu_runtime.c
│   ├── cpp_wrapper/
│   │   ├── CMakeLists.txt
│   │   ├── QemuRuntime.hpp
│   │   ├── QemuMachine.hpp
│   │   ├── QemuDisk.hpp
│   │   ├── QemuConsole.hpp
│   │   └── QemuRuntime.cpp
│   └── integration/
│       ├── CMakeLists.txt
│       ├── qemu_context.hpp
│       ├── qemu_context.cpp
│       ├── qemu_machine.cpp
│       ├── qemu_disk.cpp
│       ├── qemu_console.cpp
│       ├── qemu_event_dispatcher.cpp
│       ├── qemu_acceleration.cpp
│       ├── qemu_qmp.cpp
│       └── qemu_init_hooks.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── phase_1_build_tests/
│   ├── phase_3_4_tests/
│   ├── phase_5_tests/
│   ├── phase_6_tests/
│   ├── phase_7_tests/
│   ├── phase_8_tests/
│   ├── phase_9_tests/
│   ├── phase_10_tests/
│   ├── integration_tests/
│   ├── fixtures/
│   │   ├── kernel.bzImage
│   │   ├── rootfs.img
│   │   └── test_disk.qcow2
│   └── CMakeLists.txt
├── third_party/
│   └── qemu/                (git submodule, stable-11.0)
├── examples/
│   ├── basic_vm.cpp
│   ├── disk_io.cpp
│   ├── console_io.cpp
│   └── CMakeLists.txt
├── CMakeLists.txt
├── README.md
├── LICENSE
├── IMPLEMENTATION_PLAN.md
└── VERSION
```

---

## Build Instructions

### Linux Build

```bash
cd qvadis
mkdir -p build/linux && cd build/linux

cmake ../.. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/qemu-stable-11.0

cmake --build . -j$(nproc)
cmake --install . --prefix ./install
```

### Windows Build (MSVC)

```bash
cd qvadis
mkdir build\windows && cd build\windows

cmake ..\.. ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DCMAKE_PREFIX_PATH=C:\qemu-stable-11.0

cmake --build . --config Release
cmake --install . --prefix ./install
```

### Running Tests

```bash
cd build/linux  # or build\windows
ctest --output-on-failure -V
```

---

## Consumer Integration

### CMake FindPackage

Host applications can link `qvadis::qemu_runtime`:

```cmake
# CMakeLists.txt in host application
find_package(QemuRuntime REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE qvadis::qemu_runtime)
```

### C Example

```c
#include "qemu_runtime.h"

int main() {
    qemu_runtime_t rt = qemu_runtime_create();
    
    qemu_config_t config = {
        .machine_type = "q35",
        .memory_mb = 512,
    };
    
    qemu_runtime_init(rt, &config);
    qemu_runtime_start(rt);
    
    for (int i = 0; i < 100; i++) {
        qemu_runtime_pump_events(rt, 100);  /* 100ms timeout */
    }
    
    qemu_runtime_stop(rt);
    qemu_runtime_destroy(rt);
    return 0;
}
```

### C++ Example

```cpp
#include "QemuRuntime.hpp"
#include <iostream>

int main() {
    auto runtime = qvadis::QemuRuntime::Create();
    
    auto machine = runtime->CreateMachine("q35", 512 /* MB */);
    machine->ConfigureCPU("qemu64", 2);
    
    runtime->Start();
    
    for (int i = 0; i < 100; ++i) {
        runtime->PumpEvents(100);  // 100ms
    }
    
    runtime->Stop();
    return 0;
}
```

---

## Success Metrics

| Phase | Metric | Target |
|-------|--------|--------|
| 1-2   | Build success rate | 100% Linux & Windows |
| 3-4   | C API completeness | All lifecycle ops |
| 5     | Machine creation | <100ms |
| 6     | Disk I/O throughput | >100 MB/s |
| 7     | Console latency | <50ms per message |
| 8     | Event delivery | 100% reliability |
| 9     | KVM speedup | >10x vs TCG |
| 10    | QMP command exec | <100ms per command |
| **Overall** | **API stability** | **No breaking changes after Phase 4** |

---

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| QEMU internal API instability | Lock to stable-11.0; document internals thoroughly |
| Thread-safety issues | Extensive mutex testing; thread sanitizer in CI |
| Platform divergence | Parallel testing on Linux and Windows |
| Performance degradation | Benchmarking suite in Phase 9 |
| Memory leaks | Valgrind/DrMemory in test suite |
| API design mistakes | Design reviews before Phase 3 implementation |

---

## Timeline Estimate

| Phase | Duration | Effort (person-days) |
|-------|----------|----------------------|
| 1-2   | 1-2 weeks | 5-10 |
| 3-4   | 2-3 weeks | 10-15 |
| 5     | 1 week | 5 |
| 6     | 1 week | 5 |
| 7     | 1 week | 5 |
| 8     | 1 week | 5 |
| 9     | 2 weeks | 10 |
| 10    | 1-2 weeks | 5-10 |
| **Total** | **12-16 weeks** | **55-75 days** |

---

## Next Immediate Steps

1. **This Week**:
   - [ ] Create repository structure
   - [ ] Add QEMU as submodule at pinned commit
   - [ ] Begin Phase 1 QEMU baseline build

2. **Next Week**:
   - [ ] Document QEMU internals
   - [ ] Design and review C API
   - [ ] Begin Phase 3 implementation

3. **Phase Planning**:
   - [ ] Create detailed task breakdown for each phase
   - [ ] Assign ownership and track progress
   - [ ] Establish code review process

---

## References

- QEMU Repository: https://github.com/qemu/qemu
- QEMU Stable 11.0: https://github.com/qemu/qemu/tree/stable-11.0
- QEMU Documentation: https://www.qemu.org/docs/
- CMake Documentation: https://cmake.org/cmake/help/

---

**Document Version**: 1.0  
**Last Updated**: 2026-09-06  
**Status**: Ready for Implementation
