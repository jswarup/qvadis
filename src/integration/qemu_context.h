#pragma once

#include "qemu_runtime.h"
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <functional>

namespace qvadis {

class QemuMachineContext;

enum class RuntimeState {
    Uninitialized,
    Initialized,
    Running,
    Paused,
    Stopped
};

class QemuContext {
public:
    QemuContext();
    ~QemuContext();

    // Lifecycle
    qemu_status_t Initialize(const qemu_config_t* config);
    qemu_status_t Start();
    qemu_status_t Pause();
    qemu_status_t Resume();
    qemu_status_t Stop();

    // Event Loop
    qemu_status_t PumpEvents(uint32_t timeout_ms);

    // Queries
    bool IsRunning() const;
    bool IsPaused() const;
    RuntimeState GetState() const;

    // Machine Management
    qemu_status_t CreateMachine(const char* machine_type, QemuMachineContext** out_machine);

    // QEMU Integration Hooks
    bool InitQemuCore(int argc, char** argv);
    bool PollQemuAio(uint32_t timeout_ms);

private:
    template<typename F>
    auto WithLock(F&& f) const -> decltype(f()) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        return f();
    }

    mutable std::recursive_mutex mutex_;
    std::condition_variable_any cv_;
    std::atomic<RuntimeState> state_{RuntimeState::Uninitialized};

    // Stored configuration
    std::string machine_type_;
    std::string cpu_model_;
    uint32_t num_cpus_{1};
    uint64_t memory_mb_{512};
    std::string kernel_path_;
    std::string initrd_path_;
    std::string cmdline_;
    std::string rootfs_path_;

    std::vector<std::unique_ptr<QemuMachineContext>> machines_;
    void* qemu_aio_context_{nullptr};
};

// Machine Context representation
class QemuMachineContext {
public:
    explicit QemuMachineContext(std::string machine_type);
    ~QemuMachineContext();

    qemu_status_t ConfigureCPU(const std::string& model, uint32_t count);
    qemu_status_t ConfigureMemory(uint64_t size_mb);

    const std::string& GetMachineType() const { return machine_type_; }
    const std::string& GetCpuModel() const { return cpu_model_; }
    uint32_t GetCpuCount() const { return num_cpus_; }
    uint64_t GetMemoryMb() const { return memory_mb_; }

private:
    std::string machine_type_;
    std::string cpu_model_{"qemu64"};
    uint32_t num_cpus_{1};
    uint64_t memory_mb_{512};
};

} // namespace qvadis
