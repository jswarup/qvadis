#include "qvadis/qemu_runtime.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <string>

void test_qmp_execution() {
    std::cout << "-> Test 1: QMP command execution and JSON response handling...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    assert(rt != nullptr);

    qemu_config_t config = {};
    config.machine_type = "q35";
    config.memory_mb = 512;
    assert(qemu_runtime_init(rt, &config) == QEMU_OK);

    // Test qmp_capabilities handshake
    qemu_qmp_response_t resp = {};
    assert(qemu_runtime_execute_qmp_command(rt, "{\"execute\": \"qmp_capabilities\"}", &resp) == QEMU_OK);
    assert(resp.response != nullptr);
    assert(std::string(resp.response).find("\"return\"") != std::string::npos);
    qemu_qmp_response_free(&resp);

    // Test query-version
    assert(qemu_runtime_execute_qmp_command(rt, "{\"execute\": \"query-version\"}", &resp) == QEMU_OK);
    assert(resp.response != nullptr);
    assert(std::string(resp.response).find("qvadis-0.1.0") != std::string::npos);
    qemu_qmp_response_free(&resp);

    // Test VM control via QMP (start/stop)
    assert(qemu_runtime_start(rt) == QEMU_OK);

    // QMP query-status
    assert(qemu_runtime_execute_qmp_command(rt, "{\"execute\": \"query-status\"}", &resp) == QEMU_OK);
    assert(std::string(resp.response).find("\"running\": true") != std::string::npos);
    qemu_qmp_response_free(&resp);

    // QMP stop (pause)
    assert(qemu_runtime_execute_qmp_command(rt, "{\"execute\": \"stop\"}", &resp) == QEMU_OK);
    assert(qemu_runtime_is_paused(rt) == 1);
    qemu_qmp_response_free(&resp);

    // QMP cont (resume)
    assert(qemu_runtime_execute_qmp_command(rt, "{\"execute\": \"cont\"}", &resp) == QEMU_OK);
    assert(qemu_runtime_is_running(rt) == 1);
    qemu_qmp_response_free(&resp);

    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] QMP execution and control verified\n";
}

void test_machine_queries() {
    std::cout << "-> Test 2: Machine status and stats queries...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    qemu_machine_t machine = nullptr;
    assert(qemu_runtime_create_machine(rt, "q35", &machine) == QEMU_OK);

    char* status_json = nullptr;
    assert(qemu_machine_query_status(machine, &status_json) == QEMU_OK);
    assert(status_json != nullptr);
    assert(std::string(status_json).find("\"status\"") != std::string::npos);
    qemu_string_free(status_json);

    char* stats_json = nullptr;
    assert(qemu_machine_query_stats(machine, &stats_json) == QEMU_OK);
    assert(stats_json != nullptr);
    assert(std::string(stats_json).find("\"memory_mb\"") != std::string::npos);
    qemu_string_free(stats_json);

    qemu_machine_destroy(machine);
    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Machine queries verified\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << " Phase 10 Unit Test Suite: QMP & QAPI     \n";
    std::cout << "==========================================\n";

    test_qmp_execution();
    test_machine_queries();

    std::cout << "==========================================\n";
    std::cout << " All Phase 10 unit tests passed!\n";
    std::cout << "==========================================\n";
    return 0;
}
