# Qvadis Architecture Guide

## 1. Overview

Qvadis bridges host native applications and QEMU system emulation. By abstracting QEMU's complex initialization sequence, global BQL (Big QEMU Lock), and internal event loops into an embedded runtime library, host applications can drive virtual machines programmatically.

## 2. Layered Design

### 2.1 Public C ABI Boundary (`qemu_runtime.h`)
- Plain C99/C11 compatible header.
- Uses opaque pointers (`qemu_runtime_t`, `qemu_machine_t`, `qemu_disk_t`, `qemu_console_t`).
- Strict error code enumeration (`qemu_status_t`).
- Explicit lifecycle and ownership semantics.
- Export macros (`QEMU_RUNTIME_API`) supporting dynamic library builds on Windows (`__declspec(dllexport/dllimport)`) and POSIX (`__attribute__((visibility("default")))`).

### 2.2 C++ API Layer (`QemuRuntime.hpp`, `QemuMachine.hpp`)
- Modern C++17/C++20 wrappers living in `namespace qvadis`.
- RAII-based resource management with movable semantics and disabled copy constructors.
- Native standard library types (`std::string`, `std::vector`, `std::unique_ptr`, `std::chrono`).
- Exception-safe error propagation via status objects or exceptions.

### 2.3 Integration Layer (`src/integration/`)
- `QemuContext`: Manages runtime state transitions (`UNINITIALIZED`, `INITIALIZED`, `RUNNING`, `PAUSED`, `STOPPED`).
- Thread synchronization: Protects access to QEMU state using recursive or scoped mutexes and condition variables.
- Event loop integration: Allows host-controlled event pumping (`aio_poll` / `qemu_main_loop_wait`) or background worker threads.

## 3. Concurrency Model

QEMU's execution engine relies on:
1. **Big QEMU Lock (BQL)**: Serializes access to virtual hardware devices, memory mapping, and CPU execution management.
2. **Replay Mutex**: Manages deterministic execution or record/replay locking.
3. **AioContext**: Handles asynchronous I/O and event loop callbacks.

Qvadis manages locks cleanly across calls to `qemu_runtime_pump_events()`, `qemu_runtime_pause()`, and `qemu_runtime_resume()`.
