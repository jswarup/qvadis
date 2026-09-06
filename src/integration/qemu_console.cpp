#include "qemu_console.h"
#include <cstring>

namespace qvadis {

QemuConsole::QemuConsole(std::string device_id)
    : device_id_(std::move(device_id)) {}

QemuConsole::~QemuConsole() = default;

qemu_status_t QemuConsole::SetInputCallback(qemu_console_read_callback_t callback, void* user_data) {
    std::lock_guard<std::mutex> lock(console_mutex_);
    input_cb_ = callback;
    input_user_data_ = user_data;
    return QEMU_OK;
}

qemu_status_t QemuConsole::SetOutputCallback(qemu_console_data_cb callback, void* user_data) {
    std::lock_guard<std::mutex> lock(console_mutex_);
    output_cb_ = callback;
    output_user_data_ = user_data;
    return QEMU_OK;
}

qemu_status_t QemuConsole::Write(const uint8_t* buffer, size_t length, size_t* out_bytes_written) {
    if (!buffer || length == 0) return QEMU_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(console_mutex_);
    // Store in internal buffer
    output_buffer_.insert(output_buffer_.end(), buffer, buffer + length);

    // If output callback registered, stream to listener
    if (output_cb_) {
        output_cb_(buffer, length, output_user_data_);
    }

    if (out_bytes_written) {
        *out_bytes_written = length;
    }
    return QEMU_OK;
}

qemu_status_t QemuConsole::InjectInput(const uint8_t* buffer, size_t length) {
    if (!buffer || length == 0) return QEMU_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(console_mutex_);
    if (input_cb_) {
        input_cb_(buffer, length, input_user_data_);
    }
    return QEMU_OK;
}

} // namespace qvadis
