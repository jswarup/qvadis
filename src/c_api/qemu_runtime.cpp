#include "qemu_runtime.h"
#include "../integration/qemu_context.h"
#include "../integration/qemu_machine.h"
#include "../integration/qemu_console.h"
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

struct qemu_runtime_handle {
    std::unique_ptr<qvadis::QemuContext> ctx;
};

struct qemu_machine_handle {
    std::unique_ptr<qvadis::QemuMachineImpl> machine_impl;
};

struct qemu_disk_handle {
    qvadis::QemuDisk* disk_ptr;
};

struct qemu_console_handle {
    std::unique_ptr<qvadis::QemuConsole> console_impl;
};

extern "C" {

qemu_runtime_t qemu_runtime_create(void) {
    try {
        auto handle = new qemu_runtime_handle();
        handle->ctx = std::make_unique<qvadis::QemuContext>();
        return handle;
    } catch (...) {
        return nullptr;
    }
}

qemu_status_t qemu_runtime_init(qemu_runtime_t runtime, const qemu_config_t* config) {
    if (!runtime || !runtime->ctx) return QEMU_ERR_INVALID_ARG;
    return runtime->ctx->Initialize(config);
}

qemu_status_t qemu_runtime_start(qemu_runtime_t runtime) {
    if (!runtime || !runtime->ctx) return QEMU_ERR_INVALID_ARG;
    return runtime->ctx->Start();
}

qemu_status_t qemu_runtime_pause(qemu_runtime_t runtime) {
    if (!runtime || !runtime->ctx) return QEMU_ERR_INVALID_ARG;
    return runtime->ctx->Pause();
}

qemu_status_t qemu_runtime_resume(qemu_runtime_t runtime) {
    if (!runtime || !runtime->ctx) return QEMU_ERR_INVALID_ARG;
    return runtime->ctx->Resume();
}

qemu_status_t qemu_runtime_stop(qemu_runtime_t runtime) {
    if (!runtime || !runtime->ctx) return QEMU_ERR_INVALID_ARG;
    return runtime->ctx->Stop();
}

void qemu_runtime_destroy(qemu_runtime_t runtime) {
    if (!runtime) return;
    delete runtime;
}

qemu_status_t qemu_runtime_pump_events(qemu_runtime_t runtime, uint32_t timeout_ms) {
    if (!runtime || !runtime->ctx) return QEMU_ERR_INVALID_ARG;
    return runtime->ctx->PumpEvents(timeout_ms);
}

int qemu_runtime_is_running(qemu_runtime_t runtime) {
    if (!runtime || !runtime->ctx) return 0;
    return runtime->ctx->IsRunning() ? 1 : 0;
}

int qemu_runtime_is_paused(qemu_runtime_t runtime) {
    if (!runtime || !runtime->ctx) return 0;
    return runtime->ctx->IsPaused() ? 1 : 0;
}

const char* qemu_runtime_version(void) {
    return "0.1.0";
}

/* === Phase 5: Machine Management === */

qemu_status_t qemu_runtime_create_machine(
    qemu_runtime_t runtime,
    const char* machine_type,
    qemu_machine_t* out_machine
) {
    if (!runtime || !runtime->ctx || !out_machine) return QEMU_ERR_INVALID_ARG;

    auto m_handle = new (std::nothrow) qemu_machine_handle();
    if (!m_handle) return QEMU_ERR_OUT_OF_MEMORY;

    std::string type = machine_type ? machine_type : "q35";
    m_handle->machine_impl = std::make_unique<qvadis::QemuMachineImpl>(type);
    *out_machine = m_handle;
    return QEMU_OK;
}

qemu_status_t qemu_machine_configure_cpu(
    qemu_machine_t machine,
    const char* cpu_model,
    uint32_t num_cpus
) {
    if (!machine || !machine->machine_impl || !cpu_model) return QEMU_ERR_INVALID_ARG;
    return machine->machine_impl->ConfigureCPU(cpu_model, num_cpus);
}

qemu_status_t qemu_machine_configure_memory(
    qemu_machine_t machine,
    uint64_t size_mb
) {
    if (!machine || !machine->machine_impl) return QEMU_ERR_INVALID_ARG;
    return machine->machine_impl->ConfigureMemory(size_mb);
}

qemu_status_t qemu_machine_destroy(qemu_machine_t machine) {
    if (!machine) return QEMU_ERR_INVALID_ARG;
    delete machine;
    return QEMU_OK;
}

/* === Phase 6: Storage & Disk I/O === */

qemu_status_t qemu_machine_attach_disk(
    qemu_machine_t machine,
    const char* path,
    qemu_disk_format_t format,
    const char* device_id,
    qemu_disk_t* out_disk
) {
    if (!machine || !machine->machine_impl || !path || !device_id || !out_disk) {
        return QEMU_ERR_INVALID_ARG;
    }

    qvadis::QemuDisk* disk_ptr = nullptr;
    qemu_status_t status = machine->machine_impl->AttachDisk(path, format, device_id, &disk_ptr);
    if (status != QEMU_OK) return status;

    auto disk_handle = new (std::nothrow) qemu_disk_handle();
    if (!disk_handle) return QEMU_ERR_OUT_OF_MEMORY;

    disk_handle->disk_ptr = disk_ptr;
    *out_disk = disk_handle;
    return QEMU_OK;
}

qemu_status_t qemu_machine_detach_disk(
    qemu_machine_t machine,
    qemu_disk_t disk
) {
    if (!machine || !machine->machine_impl || !disk) return QEMU_ERR_INVALID_ARG;

    qemu_status_t status = machine->machine_impl->DetachDisk(disk->disk_ptr);
    delete disk;
    return status;
}

qemu_status_t qemu_disk_read(
    qemu_disk_t disk,
    uint64_t offset,
    uint8_t* buffer,
    size_t length,
    size_t* out_bytes_read
) {
    if (!disk || !disk->disk_ptr) return QEMU_ERR_INVALID_ARG;
    return disk->disk_ptr->Read(offset, buffer, length, out_bytes_read);
}

qemu_status_t qemu_disk_write(
    qemu_disk_t disk,
    uint64_t offset,
    const uint8_t* buffer,
    size_t length,
    size_t* out_bytes_written
) {
    if (!disk || !disk->disk_ptr) return QEMU_ERR_INVALID_ARG;
    return disk->disk_ptr->Write(offset, buffer, length, out_bytes_written);
}

/* === Phase 7: Console & Character Device Streaming === */

qemu_status_t qemu_machine_attach_console(
    qemu_machine_t machine,
    const char* console_id,
    qemu_console_data_cb on_data,
    void* user_data,
    qemu_console_t* out_console
) {
    if (!machine || !console_id || !out_console) return QEMU_ERR_INVALID_ARG;

    auto console = new (std::nothrow) qemu_console_handle();
    if (!console) return QEMU_ERR_OUT_OF_MEMORY;

    console->console_impl = std::make_unique<qvadis::QemuConsole>(console_id);
    if (on_data) {
        console->console_impl->SetOutputCallback(on_data, user_data);
    }

    *out_console = console;
    return QEMU_OK;
}

qemu_status_t qemu_console_set_input_callback(
    qemu_console_t console,
    qemu_console_read_callback_t callback,
    void* user_data
) {
    if (!console || !console->console_impl) return QEMU_ERR_INVALID_ARG;
    return console->console_impl->SetInputCallback(callback, user_data);
}

qemu_status_t qemu_console_write(
    qemu_console_t console,
    const uint8_t* buffer,
    size_t length,
    size_t* out_bytes_written
) {
    if (!console || !console->console_impl || !buffer) return QEMU_ERR_INVALID_ARG;
    return console->console_impl->Write(buffer, length, out_bytes_written);
}

/* === Phase 8: Event Callbacks & Notifications === */

qemu_status_t qemu_runtime_subscribe_event(
    qemu_runtime_t runtime,
    qemu_event_type_t event_type,
    qemu_event_callback_t callback,
    void* user_data
) {
    if (!runtime || !runtime->ctx || !callback) return QEMU_ERR_INVALID_ARG;
    return runtime->ctx->GetEventDispatcher().Subscribe(event_type, callback, user_data);
}

qemu_status_t qemu_runtime_unsubscribe_event(
    qemu_runtime_t runtime,
    qemu_event_type_t event_type,
    qemu_event_callback_t callback
) {
    if (!runtime || !runtime->ctx || !callback) return QEMU_ERR_INVALID_ARG;
    return runtime->ctx->GetEventDispatcher().Unsubscribe(event_type, callback);
}

} // extern "C"
