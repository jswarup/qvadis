#pragma once

#include "qemu_runtime.h"
#include <string>
#include <memory>
#include <stdexcept>
#include <chrono>

namespace qvadis {

class QemuMachine;

class QemuRuntime {
public:
    static std::unique_ptr<QemuRuntime> Create() {
        qemu_runtime_t handle = qemu_runtime_create();
        if (!handle) {
            throw std::runtime_error("Failed to create QemuRuntime instance");
        }
        return std::unique_ptr<QemuRuntime>(new QemuRuntime(handle));
    }

    ~QemuRuntime() {
        if (handle_) {
            qemu_runtime_destroy(handle_);
            handle_ = nullptr;
        }
    }

    // Non-copyable, movable
    QemuRuntime(const QemuRuntime&) = delete;
    QemuRuntime& operator=(const QemuRuntime&) = delete;

    QemuRuntime(QemuRuntime&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    QemuRuntime& operator=(QemuRuntime&& other) noexcept {
        if (this != &other) {
            if (handle_) qemu_runtime_destroy(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    void Initialize(const qemu_config_t& config) {
        qemu_status_t status = qemu_runtime_init(handle_, &config);
        if (status != QEMU_OK) {
            throw std::runtime_error("Failed to initialize QemuRuntime: error code " + std::to_string(status));
        }
    }

    void Start() {
        qemu_status_t status = qemu_runtime_start(handle_);
        if (status != QEMU_OK) {
            throw std::runtime_error("Failed to start QemuRuntime: error code " + std::to_string(status));
        }
    }

    void Pause() {
        qemu_status_t status = qemu_runtime_pause(handle_);
        if (status != QEMU_OK) {
            throw std::runtime_error("Failed to pause QemuRuntime: error code " + std::to_string(status));
        }
    }

    void Resume() {
        qemu_status_t status = qemu_runtime_resume(handle_);
        if (status != QEMU_OK) {
            throw std::runtime_error("Failed to resume QemuRuntime: error code " + std::to_string(status));
        }
    }

    void Stop() {
        qemu_status_t status = qemu_runtime_stop(handle_);
        if (status != QEMU_OK) {
            throw std::runtime_error("Failed to stop QemuRuntime: error code " + std::to_string(status));
        }
    }

    void PumpEvents(uint32_t timeout_ms = 0) {
        qemu_status_t status = qemu_runtime_pump_events(handle_, timeout_ms);
        if (status != QEMU_OK) {
            throw std::runtime_error("Failed to pump events: error code " + std::to_string(status));
        }
    }

    bool IsRunning() const {
        return qemu_runtime_is_running(handle_) != 0;
    }

    bool IsPaused() const {
        return qemu_runtime_is_paused(handle_) != 0;
    }

    static std::string Version() {
        return qemu_runtime_version();
    }

    qemu_runtime_t GetRawHandle() const {
        return handle_;
    }

private:
    explicit QemuRuntime(qemu_runtime_t handle) : handle_(handle) {}

    qemu_runtime_t handle_{nullptr};
};

} // namespace qvadis
