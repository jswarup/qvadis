#pragma once

#include "qemu_runtime.h"
#include <string>
#include <mutex>
#include <unordered_map>
#include <functional>

namespace qvadis {

class QemuContext;

class QemuQmp {
public:
    explicit QemuQmp(QemuContext* context);
    ~QemuQmp();

    qemu_status_t ExecuteCommand(const std::string& command_json, std::string& out_response);
    qemu_status_t QueryStatus(std::string& out_status_json);
    qemu_status_t QueryStats(std::string& out_stats_json);

private:
    std::string ProcessInternalCommand(const std::string& command_name);

    QemuContext* context_{nullptr};
    mutable std::mutex qmp_mutex_;
};

} // namespace qvadis
