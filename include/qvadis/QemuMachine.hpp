#pragma once

#include "qemu_runtime.h"
#include <string>
#include <stdexcept>
#include <memory>

namespace qvadis {

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

    qemu_machine_t GetRawHandle() const {
        return handle_;
    }

private:
    qemu_machine_t handle_{nullptr};
};

} // namespace qvadis
