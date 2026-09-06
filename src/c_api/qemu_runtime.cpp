#include "qemu_runtime.h"
#include "../integration/qemu_context.h"
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

struct qemu_runtime_handle {
    std::unique_ptr<qvadis::QemuContext> ctx;
};

struct qemu_machine_handle {
    qvadis::QemuMachineContext* machine_ctx;
};

struct qemu_disk_handle {
    char path[512];
    qemu_disk_format_t format;
    char device_id[64];
};

struct qemu_console_handle {
    char console_id[64];
    qemu_console_data_cb callback;
    void* user_data;
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

qemu_status_t qemu_runtime_create_machine(
    qemu_runtime_t runtime,
    const char* machine_type,
    qemu_machine_t* out_machine
) {
    if (!runtime || !runtime->ctx || !out_machine) return QEMU_ERR_INVALID_ARG;

    qvadis::QemuMachineContext* machine_ctx = nullptr;
    qemu_status_t status = runtime->ctx->CreateMachine(machine_type, &machine_ctx);
    if (status != QEMU_OK) return status;

    auto m_handle = new (std::nothrow) qemu_machine_handle();
    if (!m_handle) return QEMU_ERR_OUT_OF_MEMORY;

    m_handle->machine_ctx = machine_ctx;
    *out_machine = m_handle;
    return QEMU_OK;
}

qemu_status_t qemu_machine_configure_cpu(
    qemu_machine_t machine,
    const char* cpu_model,
    uint32_t num_cpus
) {
    if (!machine || !machine->machine_ctx || !cpu_model) return QEMU_ERR_INVALID_ARG;
    return machine->machine_ctx->ConfigureCPU(cpu_model, num_cpus);
}

qemu_status_t qemu_machine_configure_memory(
    qemu_machine_t machine,
    uint64_t size_mb
) {
    if (!machine || !machine->machine_ctx) return QEMU_ERR_INVALID_ARG;
    return machine->machine_ctx->ConfigureMemory(size_mb);
}

qemu_status_t qemu_machine_destroy(qemu_machine_t machine) {
    if (!machine) return QEMU_ERR_INVALID_ARG;
    delete machine;
    return QEMU_OK;
}

qemu_status_t qemu_machine_attach_disk(
    qemu_machine_t machine,
    const char* path,
    qemu_disk_format_t format,
    const char* device_id,
    qemu_disk_t* out_disk
) {
    if (!machine || !path || !device_id || !out_disk) return QEMU_ERR_INVALID_ARG;

    auto disk = new (std::nothrow) qemu_disk_handle();
    if (!disk) return QEMU_ERR_OUT_OF_MEMORY;

    std::strncpy(disk->path, path, sizeof(disk->path) - 1);
    disk->path[sizeof(disk->path) - 1] = '\0';
    disk->format = format;
    std::strncpy(disk->device_id, device_id, sizeof(disk->device_id) - 1);
    disk->device_id[sizeof(disk->device_id) - 1] = '\0';

    *out_disk = disk;
    return QEMU_OK;
}

qemu_status_t qemu_machine_detach_disk(
    qemu_machine_t machine,
    qemu_disk_t disk
) {
    if (!machine || !disk) return QEMU_ERR_INVALID_ARG;
    delete disk;
    return QEMU_OK;
}

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

    std::strncpy(console->console_id, console_id, sizeof(console->console_id) - 1);
    console->console_id[sizeof(console->console_id) - 1] = '\0';
    console->callback = on_data;
    console->user_data = user_data;

    *out_console = console;
    return QEMU_OK;
}

qemu_status_t qemu_console_write(
    qemu_console_t console,
    const uint8_t* buffer,
    size_t length,
    size_t* out_bytes_written
) {
    if (!console || !buffer) return QEMU_ERR_INVALID_ARG;

    if (console->callback) {
        console->callback(buffer, length, console->user_data);
    }
    if (out_bytes_written) {
        *out_bytes_written = length;
    }
    return QEMU_OK;
}

} // extern "C"
