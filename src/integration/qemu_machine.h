#pragma once

#include "qemu_runtime.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <fstream>
#include <unordered_map>

namespace qvadis {

class QemuDisk {
public:
    QemuDisk(std::string path, qemu_disk_format_t format, std::string device_id);
    ~QemuDisk();

    qemu_status_t Read(uint64_t offset, uint8_t* buffer, size_t length, size_t* out_bytes_read);
    qemu_status_t Write(uint64_t offset, const uint8_t* buffer, size_t length, size_t* out_bytes_written);

    const std::string& GetPath() const { return path_; }
    qemu_disk_format_t GetFormat() const { return format_; }
    const std::string& GetDeviceId() const { return device_id_; }

private:
    std::string path_;
    qemu_disk_format_t format_;
    std::string device_id_;
    mutable std::mutex disk_mutex_;
    std::vector<uint8_t> in_memory_storage_; // For virtual disks / testing buffers
};

class QemuMachineImpl {
public:
    explicit QemuMachineImpl(std::string machine_type);
    ~QemuMachineImpl();

    qemu_status_t ConfigureCPU(const std::string& model, uint32_t count);
    qemu_status_t ConfigureMemory(uint64_t size_mb);

    qemu_status_t AttachDisk(const std::string& path, qemu_disk_format_t format, const std::string& device_id, QemuDisk** out_disk);
    qemu_status_t DetachDisk(QemuDisk* disk);

    const std::string& GetMachineType() const { return machine_type_; }
    const std::string& GetCpuModel() const { return cpu_model_; }
    uint32_t GetCpuCount() const { return num_cpus_; }
    uint64_t GetMemoryMb() const { return memory_mb_; }
    size_t GetDiskCount() const;

private:
    std::string machine_type_;
    std::string cpu_model_{"qemu64"};
    uint32_t num_cpus_{1};
    uint64_t memory_mb_{512};
    mutable std::mutex machine_mutex_;
    std::unordered_map<QemuDisk*, std::unique_ptr<QemuDisk>> disks_;
};

} // namespace qvadis
