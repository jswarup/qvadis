#include "qvadis/qemu_runtime.h"
#include <iostream>
#include <cassert>
#include <atomic>

void test_event_subscription_and_dispatch() {
    std::cout << "-> Test 1: Event subscription and lifecycle notifications...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    assert(rt != nullptr);

    std::atomic<int> start_events{0};
    std::atomic<int> pause_events{0};
    std::atomic<int> stop_events{0};

    auto on_event = [](const qemu_event_t* evt, void* user_data) {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        (*counter)++;
    };

    assert(qemu_runtime_subscribe_event(rt, QEMU_EVENT_MACHINE_STARTED, on_event, &start_events) == QEMU_OK);
    assert(qemu_runtime_subscribe_event(rt, QEMU_EVENT_MACHINE_PAUSED, on_event, &pause_events) == QEMU_OK);
    assert(qemu_runtime_subscribe_event(rt, QEMU_EVENT_MACHINE_STOPPED, on_event, &stop_events) == QEMU_OK);

    qemu_config_t config = {};
    config.machine_type = "q35";
    config.memory_mb = 512;
    qemu_runtime_init(rt, &config);

    // Start -> dispatches STARTED
    assert(qemu_runtime_start(rt) == QEMU_OK);
    qemu_runtime_pump_events(rt, 10);
    assert(start_events.load() == 1);

    // Pause -> dispatches PAUSED
    assert(qemu_runtime_pause(rt) == QEMU_OK);

    // Resume -> dispatches STARTED
    assert(qemu_runtime_resume(rt) == QEMU_OK);
    qemu_runtime_pump_events(rt, 10);
    assert(pause_events.load() == 1);
    assert(start_events.load() == 2);

    // Stop -> dispatches STOPPED
    assert(qemu_runtime_stop(rt) == QEMU_OK);

    // Unsubscribe test
    assert(qemu_runtime_unsubscribe_event(rt, QEMU_EVENT_MACHINE_STARTED, on_event) == QEMU_OK);
    // Unsubscribing again should return not found
    assert(qemu_runtime_unsubscribe_event(rt, QEMU_EVENT_MACHINE_STARTED, on_event) == QEMU_ERR_NOT_FOUND);

    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Event subscription, dispatch, and unsubscription verified\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << " Phase 8 Unit Test Suite: Event Callbacks \n";
    std::cout << "==========================================\n";

    test_event_subscription_and_dispatch();

    std::cout << "==========================================\n";
    std::cout << " All Phase 8 unit tests passed!\n";
    std::cout << "==========================================\n";
    return 0;
}
