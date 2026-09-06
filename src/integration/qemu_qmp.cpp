#include "qemu_qmp.h"
#include "qemu_context.h"
#include <sstream>

namespace qvadis {

QemuQmp::QemuQmp(QemuContext* context) : context_(context) {}
QemuQmp::~QemuQmp() = default;

qemu_status_t QemuQmp::ExecuteCommand(const std::string& command_json, std::string& out_response) {
    if (command_json.empty()) return QEMU_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(qmp_mutex_);

    // Parse simple execute request: e.g. {"execute": "query-status"} or {"execute": "stop"}
    std::string cmd;
    size_t exec_pos = command_json.find("\"execute\"");
    if (exec_pos != std::string::npos) {
        size_t colon_pos = command_json.find(':', exec_pos);
        if (colon_pos != std::string::npos) {
            size_t start_quote = command_json.find('\"', colon_pos);
            if (start_quote != std::string::npos) {
                size_t end_quote = command_json.find('\"', start_quote + 1);
                if (end_quote != std::string::npos) {
                    cmd = command_json.substr(start_quote + 1, end_quote - start_quote - 1);
                }
            }
        }
    }

    if (cmd.empty()) {
        out_response = "{\"error\": {\"class\": \"GenericError\", \"desc\": \"Invalid QMP syntax or missing execute command\"}}";
        return QEMU_OK;
    }

    out_response = ProcessInternalCommand(cmd);
    return QEMU_OK;
}

std::string QemuQmp::ProcessInternalCommand(const std::string& cmd) {
    if (cmd == "qmp_capabilities") {
        return "{\"return\": {}}";
    } else if (cmd == "query-status") {
        std::string status_json;
        QueryStatus(status_json);
        return status_json;
    } else if (cmd == "query-stats") {
        std::string stats_json;
        QueryStats(stats_json);
        return stats_json;
    } else if (cmd == "stop") {
        if (context_) context_->Pause();
        return "{\"return\": {}}";
    } else if (cmd == "cont") {
        if (context_) context_->Resume();
        return "{\"return\": {}}";
    } else if (cmd == "quit") {
        if (context_) context_->Stop();
        return "{\"return\": {}}";
    } else if (cmd == "query-version") {
        return "{\"return\": {\"qemu\": {\"micro\": 0, \"minor\": 1, \"major\": 0}, \"package\": \"qvadis-0.1.0\"}}";
    } else {
        return "{\"return\": {\"status\": \"executed\", \"command\": \"" + cmd + "\"}}";
    }
}

qemu_status_t QemuQmp::QueryStatus(std::string& out_status_json) {
    std::string run_state = "prelaunch";
    bool is_running = false;
    if (context_) {
        if (context_->IsRunning()) {
            run_state = "running";
            is_running = true;
        } else if (context_->IsPaused()) {
            run_state = "paused";
        } else {
            run_state = "shutdown";
        }
    }

    out_status_json = "{\"return\": {\"status\": \"" + run_state + "\", \"singlestep\": false, \"running\": " +
                      (is_running ? "true" : "false") + "}}";
    return QEMU_OK;
}

qemu_status_t QemuQmp::QueryStats(std::string& out_stats_json) {
    out_stats_json = "{\"return\": {\"cpu_count\": 1, \"memory_mb\": 512, \"io_read_ops\": 0, \"io_write_ops\": 0}}";
    return QEMU_OK;
}

} // namespace qvadis
