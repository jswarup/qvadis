#include "qvadis/qemu_runtime.h"
#include "qvadis/QemuRuntime.hpp"
#include "qvadis/QemuMachine.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>

void test_version() {
    const char* ver = qemu_runtime_version();
    assert(ver != nullptr);
    assert(std::strcmp(ver, "0.1.0") == 0);
    std::cout << "[PASS] test_version: " << ver << "\n";
}

void test_c_api_lifecycle() {
    qemu_runtime_t rt = qemu_runtime_create();
    assert(rt != nullptr);
    assert(!qemu_runtime_is_running(rt));
    assert(!qemu_runtime_is_paused(rt));

    qemu_config_t config = {};
    config.machine_type = "q35";
    config.cpu_model = "qemu64";
    config.num_cpus = 2;
    config.memory_mb = 1024;

    qemu_status_t status = qemu_runtime_init(rt, &config);
    assert(status == QEMU_OK);

    status = qemu_runtime_start(rt);
    assert(status == QEMU_OK);
    assert(qemu_runtime_is_running(rt));
    assert(!qemu_runtime_is_paused(rt));

    // Test pumping events non-blocking
    status = qemu_runtime_pump_events(rt, 10);
    assert(status == QEMU_OK);

    status = qemu_runtime_pause(rt);
    assert(status == QEMU_OK);
    assert(!qemu_runtime_is_running(rt));
    assert(qemu_runtime_is_paused(rt));

    status = qemu_runtime_resume(rt);
    assert(status == QEMU_OK);
    assert(qemu_runtime_is_running(rt));

    status = qemu_runtime_stop(rt);
    assert(status == QEMU_OK);
    assert(!qemu_runtime_is_running(rt));

    qemu_runtime_destroy(rt);
    std::cout << "[PASS] test_c_api_lifecycle\n";
}

void test_machine_management() {
    qemu_runtime_t rt = qemu_runtime_create();
    assert(rt != nullptr);

    qemu_machine_t machine = nullptr;
    qemu_status_t status = qemu_runtime_create_machine(rt, "q35", &machine);
    assert(status == QEMU_OK);
    assert(machine != nullptr);

    status = qemu_machine_configure_cpu(machine, "host", 4);
    assert(status == QEMU_OK);

    status = qemu_machine_configure_memory(machine, 2048);
    assert(status == QEMU_OK);

    // Test disk attach
    qemu_disk_t disk = nullptr;
    status = qemu_machine_attach_disk(machine, "test_disk.qcow2", QEMU_DISK_FORMAT_QCOW2, "drive0", &disk);
    assert(status == QEMU_OK);
    assert(disk != nullptr);

    status = qemu_machine_detach_disk(machine, disk);
    assert(status == QEMU_OK);

    // Test console attach & write
    std::vector<uint8_t> captured_output;
    auto on_console = [](const uint8_t* buf, size_t len, void* user_data) {
        auto* vec = static_cast<std::vector<uint8_t>*>(user_data);
        vec->insert(vec->end(), buf, buf + len);
    };

    qemu_console_t console = nullptr;
    status = qemu_machine_attach_console(machine, "serial0", on_console, &captured_output, &console);
    assert(status == QEMU_OK);
    assert(console != nullptr);

    const char* msg = "Booting VM...\n";
    size_t written = 0;
    status = qemu_console_write(console, reinterpret_cast<const uint8_t*>(msg), std::strlen(msg), &written);
    assert(status == QEMU_OK);
    assert(written == std::strlen(msg));
    assert(captured_output.size() == std::strlen(msg));

    qemu_machine_destroy(machine);
    qemu_runtime_destroy(rt);
    std::cout << "[PASS] test_machine_management & console/storage\n";
}

void test_cpp_wrapper() {
    auto runtime = qvadis::QemuRuntime::Create();
    assert(runtime != nullptr);

    qemu_config_t config = {};
    config.machine_type = "microvm";
    config.memory_mb = 256;
    runtime->Initialize(config);

    runtime->Start();
    assert(runtime->IsRunning());

    runtime->PumpEvents(5);

    runtime->Pause();
    assert(runtime->IsPaused());

    runtime->Resume();
    assert(runtime->IsRunning());

    runtime->Stop();
    assert(!runtime->IsRunning());

    std::cout << "[PASS] test_cpp_wrapper\n";
}

int main() {
    std::cout << "Running Qvadis Test Suite...\n";
    test_version();
    test_c_api_lifecycle();
    test_machine_management();
    test_cpp_wrapper();
    std::cout << "All Qvadis tests completed successfully!\n";
    return 0;
}
