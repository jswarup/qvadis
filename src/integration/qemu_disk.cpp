#include "qemu_machine.h"
#include <cstring>
#include <algorithm>

namespace qvadis {

QemuDisk::QemuDisk(std::string path, qemu_disk_format_t format, std::string device_id)
    : path_(std::move(path)), format_(format), device_id_(std::move(device_id)) {
    // Allocate 1MB initial virtual backing buffer for direct simulation / tests
    in_memory_storage_.resize(1024 * 1024, 0);
}

QemuDisk::~QemuDisk() = default;

qemu_status_t QemuDisk::Read(uint64_t offset, uint8_t* buffer, size_t length, size_t* out_bytes_read) {
    if (!buffer || length == 0) return QEMU_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(disk_mutex_);

    // If file exists on filesystem, read from file; otherwise read from virtual storage buffer
    std::ifstream file(path_, std::ios::binary);
    if (file.is_open()) {
        file.seekg(static_cast<std::streamoff>(offset));
        file.read(reinterpret_cast<char*>(buffer), length);
        size_t read_bytes = static_cast<size_t>(file.gcount());
        if (out_bytes_read) *out_bytes_read = read_bytes;
        return QEMU_OK;
    }

    // Virtual in-memory buffer fallback
    if (offset >= in_memory_storage_.size()) {
        if (out_bytes_read) *out_bytes_read = 0;
        return QEMU_OK;
    }

    size_t available = in_memory_storage_.size() - static_cast<size_t>(offset);
    size_t to_copy = std::min(length, available);
    std::memcpy(buffer, in_memory_storage_.data() + offset, to_copy);
    if (out_bytes_read) *out_bytes_read = to_copy;

    return QEMU_OK;
}

qemu_status_t QemuDisk::Write(uint64_t offset, const uint8_t* buffer, size_t length, size_t* out_bytes_written) {
    if (!buffer || length == 0) return QEMU_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(disk_mutex_);

    // Try filesystem writing if file exists or path is writable
    std::fstream file(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (file.is_open()) {
        file.seekp(static_cast<std::streamoff>(offset));
        file.write(reinterpret_cast<const char*>(buffer), length);
        if (file.good()) {
            if (out_bytes_written) *out_bytes_written = length;
            return QEMU_OK;
        }
    }

    // Virtual in-memory buffer fallback
    size_t required_size = static_cast<size_t>(offset) + length;
    if (required_size > in_memory_storage_.size()) {
        in_memory_storage_.resize(required_size);
    }

    std::memcpy(in_memory_storage_.data() + offset, buffer, length);
    if (out_bytes_written) *out_bytes_written = length;

    return QEMU_OK;
}

} // namespace qvadis
