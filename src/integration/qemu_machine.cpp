#include "qemu_machine.h"
#include <algorithm>

namespace qvadis {

QemuMachineImpl::QemuMachineImpl(std::string machine_type)
    : machine_type_(std::move(machine_type)) {}

QemuMachineImpl::~QemuMachineImpl() = default;

qemu_status_t QemuMachineImpl::ConfigureCPU(const std::string& model, uint32_t count) {
    if (count == 0 || model.empty()) return QEMU_ERR_INVALID_ARG;
    std::lock_guard<std::mutex> lock(machine_mutex_);
    cpu_model_ = model;
    num_cpus_ = count;
    return QEMU_OK;
}

qemu_status_t QemuMachineImpl::ConfigureMemory(uint64_t size_mb) {
    if (size_mb == 0) return QEMU_ERR_INVALID_ARG;
    std::lock_guard<std::mutex> lock(machine_mutex_);
    memory_mb_ = size_mb;
    return QEMU_OK;
}

qemu_status_t QemuMachineImpl::AttachDisk(
    const std::string& path,
    qemu_disk_format_t format,
    const std::string& device_id,
    QemuDisk** out_disk
) {
    if (path.empty() || device_id.empty() || !out_disk) {
        return QEMU_ERR_INVALID_ARG;
    }

    std::lock_guard<std::mutex> lock(machine_mutex_);
    auto disk = std::make_unique<QemuDisk>(path, format, device_id);
    *out_disk = disk.get();
    disks_[disk.get()] = std::move(disk);
    return QEMU_OK;
}

qemu_status_t QemuMachineImpl::DetachDisk(QemuDisk* disk) {
    if (!disk) return QEMU_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(machine_mutex_);
    auto it = disks_.find(disk);
    if (it == disks_.end()) {
        return QEMU_ERR_NOT_FOUND;
    }

    disks_.erase(it);
    return QEMU_OK;
}

size_t QemuMachineImpl::GetDiskCount() const {
    std::lock_guard<std::mutex> lock(machine_mutex_);
    return disks_.size();
}

} // namespace qvadis
