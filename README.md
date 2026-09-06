# Qvadis: QEMU Runtime Library

Qvadis embeds QEMU into C and C++ host applications as a clean, reusable, encapsulated dynamic/shared library (`libqemuruntime.so` on Linux, `qemuruntime.dll` on Windows).

## Architecture

```
┌────────────────────────────────────────────┐
│      Host Application (CMake-based)        │
├────────────────────────────────────────────┤
│   C++ API Layer (qvadis::QemuRuntime)      │
├────────────────────────────────────────────┤
│   C ABI Boundary (qemu_runtime.h)          │
├────────────────────────────────────────────┤
│   Integration & Context Layer              │
│   (QemuContext, Event Pumping, Hooks)      │
├────────────────────────────────────────────┤
│   QEMU Core (SoftMMU / System Emulator)    │
└────────────────────────────────────────────┘
```

## Key Features

- **Stable C ABI**: Clean opaque handle interface (`qemu_runtime_t`, `qemu_machine_t`, `qemu_disk_t`, `qemu_console_t`).
- **Modern C++ Wrapper**: RAII-driven wrapper classes in `namespace qvadis`.
- **Non-blocking Event Pump**: Host application drives or steps the QEMU main event loop with timeout control.
- **Machine & Hardware Abstraction**: Configure CPUs, memory, disk I/O, and console streaming without exposing QEMU internals.
- **Thread Safety**: Mutex-guarded operations protecting QEMU execution state and event loop contexts.

## Directory Structure

```
qvadis/
├── CMakeLists.txt          # Root build configuration
├── VERSION                 # Current version
├── docs/                   # Architecture, QEMU internals, and build guides
│   ├── ARCHITECTURE.md
│   ├── QEMU_INTERNALS.md
│   └── BUILD.md
├── include/qvadis/         # Public headers
│   ├── qemu_runtime.h      # Public C ABI header
│   ├── QemuRuntime.hpp     # Modern C++ runtime wrapper
│   └── QemuMachine.hpp     # Modern C++ machine wrapper
├── src/
│   ├── c_api/              # C ABI implementation
│   ├── cpp_wrapper/        # C++ wrapper implementation
│   └── integration/        # QEMU context lifecycle & event loop hooks
├── cmake/                  # CMake helper modules (FindQEMU.cmake, etc.)
└── tests/                  # Unit and integration test suites
```

## Documentation

- [Architecture Guide](docs/ARCHITECTURE.md)
- [QEMU Internals Analysis](docs/QEMU_INTERNALS.md)
- [Building & Integrating Guide](docs/BUILD.md)
