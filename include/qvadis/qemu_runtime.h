#ifndef QEMU_RUNTIME_H
#define QEMU_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef QEMU_RUNTIME_EXPORTS
    #define QEMU_RUNTIME_API __declspec(dllexport)
  #elif defined(QEMU_RUNTIME_STATIC)
    #define QEMU_RUNTIME_API
  #else
    #define QEMU_RUNTIME_API __declspec(dllimport)
  #endif
#else
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define QEMU_RUNTIME_API __attribute__((visibility("default")))
  #else
    #define QEMU_RUNTIME_API
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle types */
typedef struct qemu_runtime_handle* qemu_runtime_t;
typedef struct qemu_machine_handle* qemu_machine_t;
typedef struct qemu_disk_handle*    qemu_disk_t;
typedef struct qemu_console_handle* qemu_console_t;

/* Status codes */
typedef enum {
    QEMU_OK = 0,
    QEMU_ERR_INVALID_ARG = 1,
    QEMU_ERR_ALREADY_RUNNING = 2,
    QEMU_ERR_NOT_RUNNING = 3,
    QEMU_ERR_INITIALIZATION = 4,
    QEMU_ERR_OUT_OF_MEMORY = 5,
    QEMU_ERR_NOT_FOUND = 6,
    QEMU_ERR_IO = 7,
    QEMU_ERR_UNKNOWN = 255
} qemu_status_t;

/* Disk format types */
typedef enum {
    QEMU_DISK_FORMAT_RAW = 0,
    QEMU_DISK_FORMAT_QCOW2 = 1,
    QEMU_DISK_FORMAT_VMDK = 2
} qemu_disk_format_t;

/* Configuration structure */
typedef struct {
    const char* machine_type;      /* e.g., "q35", "pc" */
    const char* cpu_model;         /* e.g., "qemu64", "host" */
    uint32_t    num_cpus;          /* Number of vCPUs */
    uint64_t    memory_mb;         /* RAM in megabytes */
    const char* kernel_path;       /* Optional direct kernel path */
    const char* initrd_path;       /* Optional ramdisk path */
    const char* cmdline;           /* Optional kernel command line */
    const char* rootfs_path;       /* Optional root filesystem */
} qemu_config_t;

/* Callback signature for console output streaming */
typedef void (*qemu_console_data_cb)(const uint8_t* buffer, size_t length, void* user_data);

/* === Runtime Lifecycle === */

/**
 * Create a new QEMU runtime instance
 * @return Opaque handle to runtime, NULL on allocation failure
 */
QEMU_RUNTIME_API qemu_runtime_t qemu_runtime_create(void);

/**
 * Initialize runtime with configuration
 * @param runtime Target runtime instance
 * @param config Pointer to configuration struct
 * @return Status code
 */
QEMU_RUNTIME_API qemu_status_t qemu_runtime_init(qemu_runtime_t runtime, const qemu_config_t* config);

/**
 * Start emulation
 * @param runtime Target runtime instance
 * @return Status code
 */
QEMU_RUNTIME_API qemu_status_t qemu_runtime_start(qemu_runtime_t runtime);

/**
 * Pause emulation (non-blocking)
 * @param runtime Target runtime instance
 * @return Status code
 */
QEMU_RUNTIME_API qemu_status_t qemu_runtime_pause(qemu_runtime_t runtime);

/**
 * Resume emulation after pause
 * @param runtime Target runtime instance
 * @return Status code
 */
QEMU_RUNTIME_API qemu_status_t qemu_runtime_resume(qemu_runtime_t runtime);

/**
 * Stop emulation
 * @param runtime Target runtime instance
 * @return Status code
 */
QEMU_RUNTIME_API qemu_status_t qemu_runtime_stop(qemu_runtime_t runtime);

/**
 * Destroy runtime and free resources
 * @param runtime Target runtime instance
 */
QEMU_RUNTIME_API void qemu_runtime_destroy(qemu_runtime_t runtime);

/* === Event Loop Integration === */

/**
 * Run QEMU event loop for specified milliseconds.
 * Non-blocking if timeout_ms = 0.
 * @param runtime Target runtime instance
 * @param timeout_ms Max milliseconds to wait for events
 * @return Status code
 */
QEMU_RUNTIME_API qemu_status_t qemu_runtime_pump_events(qemu_runtime_t runtime, uint32_t timeout_ms);

/**
 * Query if emulation is currently running
 */
QEMU_RUNTIME_API int qemu_runtime_is_running(qemu_runtime_t runtime);

/**
 * Query if emulation is currently paused
 */
QEMU_RUNTIME_API int qemu_runtime_is_paused(qemu_runtime_t runtime);

/**
 * Return library version string
 */
QEMU_RUNTIME_API const char* qemu_runtime_version(void);

/* === Machine Management === */

/**
 * Create or get machine handle from runtime
 */
QEMU_RUNTIME_API qemu_status_t qemu_runtime_create_machine(
    qemu_runtime_t runtime,
    const char* machine_type,
    qemu_machine_t* out_machine
);

QEMU_RUNTIME_API qemu_status_t qemu_machine_configure_cpu(
    qemu_machine_t machine,
    const char* cpu_model,
    uint32_t num_cpus
);

QEMU_RUNTIME_API qemu_status_t qemu_machine_configure_memory(
    qemu_machine_t machine,
    uint64_t size_mb
);

QEMU_RUNTIME_API qemu_status_t qemu_machine_destroy(qemu_machine_t machine);

/* === Storage Management === */

QEMU_RUNTIME_API qemu_status_t qemu_machine_attach_disk(
    qemu_machine_t machine,
    const char* path,
    qemu_disk_format_t format,
    const char* device_id,
    qemu_disk_t* out_disk
);

QEMU_RUNTIME_API qemu_status_t qemu_machine_detach_disk(
    qemu_machine_t machine,
    qemu_disk_t disk
);

/* === Console / Chardev Management === */

QEMU_RUNTIME_API qemu_status_t qemu_machine_attach_console(
    qemu_machine_t machine,
    const char* console_id,
    qemu_console_data_cb on_data,
    void* user_data,
    qemu_console_t* out_console
);

QEMU_RUNTIME_API qemu_status_t qemu_console_write(
    qemu_console_t console,
    const uint8_t* buffer,
    size_t length,
    size_t* out_bytes_written
);

#ifdef __cplusplus
}
#endif

#endif /* QEMU_RUNTIME_H */
