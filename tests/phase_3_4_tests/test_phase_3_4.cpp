#include "qvadis/qemu_runtime.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

void test_create_and_destroy() {
    std::cout << "-> Test 1: C API create and destroy...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    assert(rt != nullptr);
    assert(qemu_runtime_is_running(rt) == 0);
    assert(qemu_runtime_is_paused(rt) == 0);
    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Created and destroyed cleanly\n";
}

void test_initialization() {
    std::cout << "-> Test 2: C API initialization validation...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    assert(rt != nullptr);

    // Null config should fail with invalid arg
    qemu_status_t status = qemu_runtime_init(rt, nullptr);
    assert(status == QEMU_ERR_INVALID_ARG);

    qemu_config_t config = {};
    config.machine_type = "q35";
    config.cpu_model = "qemu64";
    config.num_cpus = 4;
    config.memory_mb = 2048;

    status = qemu_runtime_init(rt, &config);
    assert(status == QEMU_OK);

    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Initialization validated\n";
}

void test_full_lifecycle() {
    std::cout << "-> Test 3: Lifecycle state transitions (Init -> Start -> Pause -> Resume -> Stop)...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    assert(rt != nullptr);

    // Starting before init should fail
    qemu_status_t status = qemu_runtime_start(rt);
    assert(status == QEMU_ERR_INITIALIZATION);

    qemu_config_t config = {};
    config.machine_type = "pc";
    config.memory_mb = 1024;
    assert(qemu_runtime_init(rt, &config) == QEMU_OK);

    // Start
    assert(qemu_runtime_start(rt) == QEMU_OK);
    assert(qemu_runtime_is_running(rt) == 1);
    assert(qemu_runtime_is_paused(rt) == 0);

    // Starting already running runtime should fail
    assert(qemu_runtime_start(rt) == QEMU_ERR_ALREADY_RUNNING);

    // Pause
    assert(qemu_runtime_pause(rt) == QEMU_OK);
    assert(qemu_runtime_is_running(rt) == 0);
    assert(qemu_runtime_is_paused(rt) == 1);

    // Resume
    assert(qemu_runtime_resume(rt) == QEMU_OK);
    assert(qemu_runtime_is_running(rt) == 1);
    assert(qemu_runtime_is_paused(rt) == 0);

    // Stop
    assert(qemu_runtime_stop(rt) == QEMU_OK);
    assert(qemu_runtime_is_running(rt) == 0);
    assert(qemu_runtime_is_paused(rt) == 0);

    // Stopping already stopped runtime
    assert(qemu_runtime_stop(rt) == QEMU_ERR_NOT_RUNNING);

    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] State transitions verified\n";
}

void test_event_pumping() {
    std::cout << "-> Test 4: Event loop pumping (non-blocking & with timeout)...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    qemu_config_t config = {};
    config.machine_type = "q35";
    config.memory_mb = 512;
    qemu_runtime_init(rt, &config);

    // Cannot pump when not running
    assert(qemu_runtime_pump_events(rt, 0) == QEMU_ERR_NOT_RUNNING);

    assert(qemu_runtime_start(rt) == QEMU_OK);

    // Non-blocking pump
    assert(qemu_runtime_pump_events(rt, 0) == QEMU_OK);

    // Timed pump
    assert(qemu_runtime_pump_events(rt, 20) == QEMU_OK);

    assert(qemu_runtime_stop(rt) == QEMU_OK);
    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Event pump works as expected\n";
}

void test_thread_safety() {
    std::cout << "-> Test 5: Concurrent multi-threaded access and event pumping...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    qemu_config_t config = {};
    config.machine_type = "q35";
    config.memory_mb = 512;
    qemu_runtime_init(rt, &config);
    qemu_runtime_start(rt);

    std::atomic<bool> done{false};
    std::atomic<int> pump_count{0};

    // Worker thread 1: Pumping events continuously
    std::thread pumper([&]() {
        while (!done.load()) {
            if (qemu_runtime_pump_events(rt, 5) == QEMU_OK) {
                pump_count++;
            }
        }
    });

    // Worker thread 2: Querying status continuously
    std::thread reader([&]() {
        while (!done.load()) {
            qemu_runtime_is_running(rt);
            qemu_runtime_is_paused(rt);
            std::this_thread::yield();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    done.store(true);

    pumper.join();
    reader.join();

    assert(pump_count.load() > 0);
    assert(qemu_runtime_stop(rt) == QEMU_OK);
    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Thread-safe concurrency verified (pump count: " << pump_count.load() << ")\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << " Phase 3-4 Unit Test Suite: C API & Loop  \n";
    std::cout << "==========================================\n";

    test_create_and_destroy();
    test_initialization();
    test_full_lifecycle();
    test_event_pumping();
    test_thread_safety();

    std::cout << "==========================================\n";
    std::cout << " All Phase 3-4 unit tests passed!\n";
    std::cout << "==========================================\n";
    return 0;
}
