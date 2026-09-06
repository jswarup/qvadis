#include "qvadis/qemu_runtime.h"
#include "qvadis/QemuMachine.hpp"
#include <iostream>
#include <cassert>
#include <cstring>

void test_machine_c_api() {
    std::cout << "-> Test 1: Machine C API creation, configuration and destruction...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    assert(rt != nullptr);

    qemu_machine_t machine = nullptr;
    assert(qemu_runtime_create_machine(rt, "q35", &machine) == QEMU_OK);
    assert(machine != nullptr);

    // Invalid CPU config
    assert(qemu_machine_configure_cpu(machine, nullptr, 2) == QEMU_ERR_INVALID_ARG);
    assert(qemu_machine_configure_cpu(machine, "host", 0) == QEMU_ERR_INVALID_ARG);

    // Valid CPU config
    assert(qemu_machine_configure_cpu(machine, "host", 4) == QEMU_OK);

    // Invalid memory config
    assert(qemu_machine_configure_memory(machine, 0) == QEMU_ERR_INVALID_ARG);

    // Valid memory config
    assert(qemu_machine_configure_memory(machine, 4096) == QEMU_OK);

    assert(qemu_machine_destroy(machine) == QEMU_OK);
    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Machine C API verified\n";
}

void test_machine_cpp_wrapper() {
    std::cout << "-> Test 2: Machine C++ RAII wrapper...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    {
        auto machine = qvadis::QemuMachine::Create(rt, "microvm");
        assert(machine != nullptr);
        machine->ConfigureCPU("qemu64", 2);
        machine->ConfigureMemory(1024);

        // Test move semantics
        qvadis::QemuMachine moved_machine = std::move(*machine);
        assert(moved_machine.GetRawHandle() != nullptr);
    }
    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Machine C++ wrapper verified\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << " Phase 5 Unit Test Suite: Machine Mgmt   \n";
    std::cout << "==========================================\n";

    test_machine_c_api();
    test_machine_cpp_wrapper();

    std::cout << "==========================================\n";
    std::cout << " All Phase 5 unit tests passed!\n";
    std::cout << "==========================================\n";
    return 0;
}
