#include "qvadis/qemu_runtime.h"
#include "qvadis/QemuConsole.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>

void test_console_streaming() {
    std::cout << "-> Test 1: Console output streaming via callback...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    qemu_machine_t machine = nullptr;
    assert(qemu_runtime_create_machine(rt, "q35", &machine) == QEMU_OK);

    std::vector<uint8_t> output_stream;
    auto on_output = [](const uint8_t* data, size_t len, void* user_data) {
        auto* vec = static_cast<std::vector<uint8_t>*>(user_data);
        vec->insert(vec->end(), data, data + len);
    };

    qemu_console_t console = nullptr;
    assert(qemu_machine_attach_console(machine, "serial0", on_output, &output_stream, &console) == QEMU_OK);
    assert(console != nullptr);

    const char* boot_msg = "SeaBIOS booting Linux kernel...\n";
    size_t written = 0;
    assert(qemu_console_write(console, reinterpret_cast<const uint8_t*>(boot_msg), std::strlen(boot_msg), &written) == QEMU_OK);
    assert(written == std::strlen(boot_msg));
    assert(output_stream.size() == std::strlen(boot_msg));
    assert(std::memcmp(output_stream.data(), boot_msg, written) == 0);

    qemu_machine_destroy(machine);
    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Console streaming works\n";
}

void test_console_input_callback() {
    std::cout << "-> Test 2: Console input callback registration...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    qemu_machine_t machine = nullptr;
    assert(qemu_runtime_create_machine(rt, "q35", &machine) == QEMU_OK);

    qemu_console_t console = nullptr;
    assert(qemu_machine_attach_console(machine, "serial0", nullptr, nullptr, &console) == QEMU_OK);

    bool callback_invoked = false;
    auto on_input = [](const uint8_t* data, size_t len, void* user_data) {
        auto* flag = static_cast<bool*>(user_data);
        *flag = true;
    };

    assert(qemu_console_set_input_callback(console, on_input, &callback_invoked) == QEMU_OK);

    qemu_machine_destroy(machine);
    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Console input registration verified\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << " Phase 7 Unit Test Suite: Console & Stream\n";
    std::cout << "==========================================\n";

    test_console_streaming();
    test_console_input_callback();

    std::cout << "==========================================\n";
    std::cout << " All Phase 7 unit tests passed!\n";
    std::cout << "==========================================\n";
    return 0;
}
