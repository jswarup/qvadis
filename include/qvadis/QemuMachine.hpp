#pragma once

#include "qemu_runtime.h"
#include <string>
#include <stdexcept>
#include <memory>
#include <vector>

namespace qvadis {

class QemuDiskHandle {
public:
    explicit QemuDiskHandle(qemu_disk_t handle) : handle_(handle) {}
    ~QemuDiskHandle() = default;

    size_t Read(uint64_t offset, uint8_t* buffer, size_t length) {
        size_t read_bytes = 0;
        qemu_status_t status = qemu_disk_read(handle_, offset, buffer, length, &read_bytes);
        if (status != QEMU_OK) {
            throw std::runtime_error("Disk read failed: code " + std::to_string(status));
        }
        return read_bytes;
    }

    size_t Write(uint64_t offset, const uint8_t* buffer, size_t length) {
        size_t written_bytes = 0;
        qemu_status_t status = qemu_disk_write(handle_, offset, buffer, length, &written_bytes);
        if (status != QEMU_OK) {
            throw std::runtime_error("Disk write failed: code " + std::to_string(status));
        }
        return written_bytes;
    }

    qemu_disk_t GetRawHandle() const { return handle_; }

private:
    qemu_disk_t handle_{nullptr};
};

class QemuMachine {
public:
    static std::unique_ptr<QemuMachine> Create(qemu_runtime_t runtime, const std::string& machine_type) {
        qemu_machine_t handle = nullptr;
        qemu_status_t status = qemu_runtime_create_machine(runtime, machine_type.c_str(), &handle);
        if (status != QEMU_OK || !handle) {
            throw std::runtime_error("Failed to create QemuMachine: error code " + std::to_string(status));
        }
        return std::unique_ptr<QemuMachine>(new QemuMachine(handle));
    }

    explicit QemuMachine(qemu_machine_t handle) : handle_(handle) {}

    ~QemuMachine() {
        if (handle_) {
            qemu_machine_destroy(handle_);
            handle_ = nullptr;
        }
    }

    QemuMachine(const QemuMachine&) = delete;
    QemuMachine& operator=(const QemuMachine&) = delete;

    QemuMachine(QemuMachine&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    QemuMachine& operator=(QemuMachine&& other) noexcept {
        if (this != &other) {
            if (handle_) qemu_machine_destroy(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    void ConfigureCPU(const std::string& model, uint32_t count) {
        qemu_status_t status = qemu_machine_configure_cpu(handle_, model.c_str(), count);
        if (status != QEMU_OK) {
            throw std::runtime_error("Failed to configure CPU: error code " + std::to_string(status));
        }
    }

    void ConfigureMemory(uint64_t size_mb) {
        qemu_status_t status = qemu_machine_configure_memory(handle_, size_mb);
        if (status != QEMU_OK) {
            throw std::runtime_error("Failed to configure memory: error code " + std::to_string(status));
        }
    }

    qemu_disk_t AttachDisk(const std::string& path, qemu_disk_format_t format, const std::string& device_id) {
        qemu_disk_t disk = nullptr;
        qemu_status_t status = qemu_machine_attach_disk(handle_, path.c_str(), format, device_id.c_str(), &disk);
        if (status != QEMU_OK || !disk) {
            throw std::runtime_error("Failed to attach disk: error code " + std::to_string(status));
        }
        return disk;
    }

    void DetachDisk(qemu_disk_t disk) {
        qemu_status_t status = qemu_machine_detach_disk(handle_, disk);
        if (status != QEMU_OK) {
            throw std::runtime_error("Failed to detach disk: error code " + std::to_string(status));
        }
    }

    qemu_machine_t GetRawHandle() const {
        return handle_;
    }

private:
    qemu_machine_t handle_{nullptr};
};

} // namespace qvadis
