#include "qemu_context.h"
#include <chrono>
#include <thread>
#include <iostream>

namespace qvadis {

QemuContext::QemuContext() = default;

QemuContext::~QemuContext() {
    if (state_ == RuntimeState::Running || state_ == RuntimeState::Paused) {
        Stop();
    }
}

qemu_status_t QemuContext::Initialize(const qemu_config_t* config) {
    if (!config) {
        return QEMU_ERR_INVALID_ARG;
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state_ == RuntimeState::Running) {
        return QEMU_ERR_ALREADY_RUNNING;
    }

    machine_type_ = config->machine_type ? config->machine_type : "q35";
    cpu_model_    = config->cpu_model ? config->cpu_model : "qemu64";
    num_cpus_     = config->num_cpus > 0 ? config->num_cpus : 1;
    memory_mb_    = config->memory_mb > 0 ? config->memory_mb : 512;
    kernel_path_  = config->kernel_path ? config->kernel_path : "";
    initrd_path_  = config->initrd_path ? config->initrd_path : "";
    cmdline_      = config->cmdline ? config->cmdline : "";
    rootfs_path_  = config->rootfs_path ? config->rootfs_path : "";

    state_ = RuntimeState::Initialized;
    return QEMU_OK;
}

qemu_status_t QemuContext::Start() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state_ == RuntimeState::Running) {
        return QEMU_ERR_ALREADY_RUNNING;
    }
    if (state_ != RuntimeState::Initialized && state_ != RuntimeState::Stopped && state_ != RuntimeState::Paused) {
        return QEMU_ERR_INITIALIZATION;
    }

    state_ = RuntimeState::Running;
    cv_.notify_all();
    return QEMU_OK;
}

qemu_status_t QemuContext::Pause() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state_ != RuntimeState::Running) {
        return QEMU_ERR_NOT_RUNNING;
    }

    state_ = RuntimeState::Paused;
    cv_.notify_all();
    return QEMU_OK;
}

qemu_status_t QemuContext::Resume() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state_ != RuntimeState::Paused) {
        return QEMU_ERR_INVALID_ARG;
    }

    state_ = RuntimeState::Running;
    cv_.notify_all();
    return QEMU_OK;
}

qemu_status_t QemuContext::Stop() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state_ != RuntimeState::Running && state_ != RuntimeState::Paused) {
        return QEMU_ERR_NOT_RUNNING;
    }

    state_ = RuntimeState::Stopped;
    cv_.notify_all();
    return QEMU_OK;
}

qemu_status_t QemuContext::PumpEvents(uint32_t timeout_ms) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state_ != RuntimeState::Running) {
        return QEMU_ERR_NOT_RUNNING;
    }

    if (timeout_ms > 0) {
        // Sleep or wait on condition variable up to timeout
        // In full QEMU integration, this invokes aio_poll / main_loop_wait
        std::unique_lock<std::recursive_mutex> ulock(mutex_, std::adopt_lock);
        cv_.wait_for(ulock, std::chrono::milliseconds(timeout_ms), [this] {
            return state_ != RuntimeState::Running;
        });
        ulock.release(); // release adoption since lock was already held
    }

    return QEMU_OK;
}

bool QemuContext::IsRunning() const {
    return state_ == RuntimeState::Running;
}

bool QemuContext::IsPaused() const {
    return state_ == RuntimeState::Paused;
}

RuntimeState QemuContext::GetState() const {
    return state_.load();
}

qemu_status_t QemuContext::CreateMachine(const char* machine_type, QemuMachineContext** out_machine) {
    if (!out_machine) return QEMU_ERR_INVALID_ARG;

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string type = machine_type ? machine_type : machine_type_;
    auto machine = std::make_unique<QemuMachineContext>(type);
    *out_machine = machine.get();
    machines_.push_back(std::move(machine));
    return QEMU_OK;
}

// QemuMachineContext
QemuMachineContext::QemuMachineContext(std::string machine_type)
    : machine_type_(std::move(machine_type)) {}

QemuMachineContext::~QemuMachineContext() = default;

qemu_status_t QemuMachineContext::ConfigureCPU(const std::string& model, uint32_t count) {
    if (count == 0) return QEMU_ERR_INVALID_ARG;
    cpu_model_ = model;
    num_cpus_ = count;
    return QEMU_OK;
}

qemu_status_t QemuMachineContext::ConfigureMemory(uint64_t size_mb) {
    if (size_mb == 0) return QEMU_ERR_INVALID_ARG;
    memory_mb_ = size_mb;
    return QEMU_OK;
}

} // namespace qvadis
