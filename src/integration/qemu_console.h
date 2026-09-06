#pragma once

#include "qemu_runtime.h"
#include <string>
#include <vector>
#include <mutex>
#include <functional>

namespace qvadis {

class QemuConsole {
public:
    explicit QemuConsole(std::string device_id);
    ~QemuConsole();

    qemu_status_t SetInputCallback(qemu_console_read_callback_t callback, void* user_data);
    qemu_status_t SetOutputCallback(qemu_console_data_cb callback, void* user_data);

    qemu_status_t Write(const uint8_t* buffer, size_t length, size_t* out_bytes_written);
    qemu_status_t InjectInput(const uint8_t* buffer, size_t length);

    const std::string& GetDeviceId() const { return device_id_; }

private:
    std::string device_id_;
    mutable std::mutex console_mutex_;

    qemu_console_read_callback_t input_cb_{nullptr};
    void* input_user_data_{nullptr};

    qemu_console_data_cb output_cb_{nullptr};
    void* output_user_data_{nullptr};

    std::vector<uint8_t> output_buffer_;
};

} // namespace qvadis
