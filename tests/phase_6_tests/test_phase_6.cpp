#include "qvadis/qemu_runtime.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>

void test_disk_attach_and_detach() {
    std::cout << "-> Test 1: Virtual disk attach and detach...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    qemu_machine_t machine = nullptr;
    assert(qemu_runtime_create_machine(rt, "q35", &machine) == QEMU_OK);

    qemu_disk_t disk = nullptr;
    assert(qemu_machine_attach_disk(machine, "disk0.raw", QEMU_DISK_FORMAT_RAW, "drive0", &disk) == QEMU_OK);
    assert(disk != nullptr);

    assert(qemu_machine_detach_disk(machine, disk) == QEMU_OK);

    qemu_machine_destroy(machine);
    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Disk attach and detach successful\n";
}

void test_disk_read_write() {
    std::cout << "-> Test 2: Disk synchronous read and write operations...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    qemu_machine_t machine = nullptr;
    assert(qemu_runtime_create_machine(rt, "q35", &machine) == QEMU_OK);

    qemu_disk_t disk = nullptr;
    assert(qemu_machine_attach_disk(machine, "virtual_test.qcow2", QEMU_DISK_FORMAT_QCOW2, "drive1", &disk) == QEMU_OK);

    const char* write_payload = "QEMU_STORAGE_BLOCK_DATA_TEST_1234567890";
    size_t payload_len = std::strlen(write_payload);
    size_t written = 0;

    assert(qemu_disk_write(disk, 0x1000, reinterpret_cast<const uint8_t*>(write_payload), payload_len, &written) == QEMU_OK);
    assert(written == payload_len);

    std::vector<uint8_t> read_buffer(payload_len, 0);
    size_t read_bytes = 0;
    assert(qemu_disk_read(disk, 0x1000, read_buffer.data(), payload_len, &read_bytes) == QEMU_OK);
    assert(read_bytes == payload_len);
    assert(std::memcmp(read_buffer.data(), write_payload, payload_len) == 0);

    assert(qemu_machine_detach_disk(machine, disk) == QEMU_OK);

    qemu_machine_destroy(machine);
    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Read/write integrity verified\n";
}

void test_multiple_disks() {
    std::cout << "-> Test 3: Multiple concurrent attached disks...\n";
    qemu_runtime_t rt = qemu_runtime_create();
    qemu_machine_t machine = nullptr;
    assert(qemu_runtime_create_machine(rt, "q35", &machine) == QEMU_OK);

    qemu_disk_t disk1 = nullptr;
    qemu_disk_t disk2 = nullptr;
    qemu_disk_t disk3 = nullptr;

    assert(qemu_machine_attach_disk(machine, "boot.raw", QEMU_DISK_FORMAT_RAW, "hda", &disk1) == QEMU_OK);
    assert(qemu_machine_attach_disk(machine, "data.qcow2", QEMU_DISK_FORMAT_QCOW2, "hdb", &disk2) == QEMU_OK);
    assert(qemu_machine_attach_disk(machine, "backup.vmdk", QEMU_DISK_FORMAT_VMDK, "hdc", &disk3) == QEMU_OK);

    // Independent writes to different disks
    const char* data1 = "BOOT_SECTOR";
    const char* data2 = "DATA_PAYLOAD";
    size_t written = 0;
    assert(qemu_disk_write(disk1, 0, reinterpret_cast<const uint8_t*>(data1), std::strlen(data1), &written) == QEMU_OK);
    assert(qemu_disk_write(disk2, 0, reinterpret_cast<const uint8_t*>(data2), std::strlen(data2), &written) == QEMU_OK);

    // Detach all
    assert(qemu_machine_detach_disk(machine, disk1) == QEMU_OK);
    assert(qemu_machine_detach_disk(machine, disk2) == QEMU_OK);
    assert(qemu_machine_detach_disk(machine, disk3) == QEMU_OK);

    qemu_machine_destroy(machine);
    qemu_runtime_destroy(rt);
    std::cout << "   [PASS] Multiple disks attached and detached cleanly\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << " Phase 6 Unit Test Suite: Storage & Disk  \n";
    std::cout << "==========================================\n";

    test_disk_attach_and_detach();
    test_disk_read_write();
    test_multiple_disks();

    std::cout << "==========================================\n";
    std::cout << " All Phase 6 unit tests passed!\n";
    std::cout << "==========================================\n";
    return 0;
}
