#pragma once

#include "qemu_runtime.h"
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

namespace qvadis {

class QemuConsoleHandle {
public:
    explicit QemuConsoleHandle(qemu_console_t handle) : handle_(handle) {}
    ~QemuConsoleHandle() = default;

    void SetInputCallback(qemu_console_read_callback_t cb, void* user_data) {
        qemu_status_t status = qemu_console_set_input_callback(handle_, cb, user_data);
        if (status != QEMU_OK) {
            throw std::runtime_error("Failed to set console input callback: " + std::to_string(status));
        }
    }

    size_t Write(const std::string& text) {
        size_t written = 0;
        qemu_status_t status = qemu_console_write(
            handle_,
            reinterpret_cast<const uint8_t*>(text.data()),
            text.size(),
            &written
        );
        if (status != QEMU_OK) {
            throw std::runtime_error("Failed to write to console: " + std::to_string(status));
        }
        return written;
    }

    qemu_console_t GetRawHandle() const { return handle_; }

private:
    qemu_console_t handle_{nullptr};
};

} // namespace qvadis
