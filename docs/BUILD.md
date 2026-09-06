# Building Qvadis

## Prerequisites

- **CMake**: version 3.20 or newer (tested with 4.4.2)
- **C++ Compiler**: Modern C++17 compatible compiler:
  - MSVC 2019 / 2022 / 2026 (Windows)
  - GCC 9+ or Clang 10+ (Linux)
- **Build System**: Ninja or Make / MSBuild
- **QEMU Dependencies**:
  - GLib 2.0 (`libglib-2.0`)
  - Pixman (`libpixman-1`)

## Building on Windows

From a Visual Studio Developer Command Prompt or PowerShell with MSVC tools:

```powershell
cd qvadis
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

To run tests:
```powershell
ctest --test-dir build --output-on-failure
```

## Building on Linux

```bash
cd qvadis
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
ctest --output-on-failure
```

## Integration with CMake Projects

```cmake
find_package(QemuRuntime REQUIRED)

add_executable(my_host_app main.cpp)
target_link_libraries(my_host_app PRIVATE qvadis::qemu_runtime)
```
