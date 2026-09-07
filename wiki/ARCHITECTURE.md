# Qvadis Architecture Guide

## 1. Overview

Qvadis bridges host native applications and QEMU system emulation. By abstracting QEMU's complex initialization sequence, global BQL (Big QEMU Lock), and internal event loops into an embedded runtime library, host applications can drive virtual machines programmatically.

## 2. Services Provided

### 2.1 Host-Side Services
The library provides the host application with the following programmatic services to control and inspect the virtual machine:

* **Lifecycle & State Management**: Programmatic controls to initialize, start, pause, resume, and stop the virtual machine via functions like `qemu_runtime_start`, `qemu_runtime_pause`, and `qemu_runtime_resume`.
* **Machine Configuration**: APIs to pre-configure CPU models, core counts, RAM size, and machine types (`qemu_machine_configure_cpu`, `qemu_machine_configure_memory`).
* **Storage Management**: Capabilities to attach and detach virtual disks (`qemu_machine_attach_disk`). Provides host-side raw byte I/O operations (`qemu_disk_read`, `qemu_disk_write`) to directly manipulate virtual disk contents while the VM runs.
* **Console I/O Streaming**: Stream guest console and character device output directly into host memory via callbacks (`qemu_machine_attach_console`). Conversely, inject host input bytes directly into the guest terminal (`qemu_console_write`).
* **Event Dispatching**: A publish-subscribe event model (`QemuEventDispatcher`) for real-time notifications of VM lifecycle changes (STARTED, PAUSED, STOPPED) and asynchronous I/O completion.
* **QMP (QEMU Monitor Protocol) Integration**: Execution of JSON-based management commands to query internal states, hot-plug devices, and retrieve execution statistics directly from the host.

### 2.2 VM-Side Services
From the guest Virtual Machine's perspective, the library provides the following emulated capabilities and abstractions:

* **Emulated Hardware Platform**: A fully functional emulated motherboard (e.g., `q35` or `pc`), providing standard PCI/PCIe buses, memory-mapped I/O, timers, and CPU instruction set architectures.
* **Virtual CPU & Memory**: Provisioning of execution units (vCPUs) with specific feature flags (e.g., `qemu64`) and physical memory backing (RAM).
* **Virtual Block Storage**: Presentation of host-attached files or buffers as virtual block devices (virtio-blk or IDE), allowing the guest OS to mount, read, and write persistent filesystems.
* **Serial/Character Devices**: Presentation of serial ports (ttyS0) and virtio-consoles to the guest OS, enabling kernel boot logging, TTY terminals, and bidirectional stream communication with the host.

## 3. Layered Design

### 3.1 Public C ABI Boundary (`qemu_runtime.h`)
- Plain C99/C11 compatible header.
- Uses opaque pointers (`qemu_runtime_t`, `qemu_machine_t`, `qemu_disk_t`, `qemu_console_t`) to hide C++ and QEMU internals.
- Strict error code enumeration (`qemu_status_t`).
- Explicit lifecycle and ownership semantics.
- Export macros (`QEMU_RUNTIME_API`) supporting dynamic library builds on Windows (`__declspec(dllexport/dllimport)`) and POSIX (`__attribute__((visibility("default")))`).

### 3.2 C++ API Layer (`QemuRuntime.hpp`, `QemuMachine.hpp`)
- Modern C++17/C++20 wrappers living in `namespace qvadis`.
- RAII-based resource management with movable semantics and disabled copy constructors.
- Native standard library types (`std::string`, `std::vector`, `std::unique_ptr`, `std::chrono`).
- Exception-safe error propagation via status objects or exceptions.

### 3.3 Integration Layer (`src/integration/`)
- `QemuContext`: Core engine that manages runtime state transitions (`Uninitialized`, `Initialized`, `Running`, `Paused`, `Stopped`).
- `QemuDisk` & `QemuConsole`: Pluggable device implementations that wrap QEMU's block and chardev backends respectively.
- `QemuQmp`: Manages JSON serialization and dispatch to QEMU's monitor subsystem.

## 4. Concurrency Model

QEMU's execution engine relies on:
1. **Big QEMU Lock (BQL)**: Serializes access to virtual hardware devices, memory mapping, and CPU execution management.
2. **Replay Mutex**: Manages deterministic execution or record/replay locking.
3. **AioContext**: Handles asynchronous I/O and event loop callbacks.

Qvadis manages host-side concurrency primarily through a `std::recursive_mutex` in `QemuContext`, ensuring re-entrant safety. It safely handles lock acquisition across calls to `qemu_runtime_pump_events()`, using `std::condition_variable_any` to allow thread suspension and wake-ups when asynchronous events (like state transitions) are fired.
