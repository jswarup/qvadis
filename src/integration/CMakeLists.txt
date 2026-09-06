add_library(qvadis_integration OBJECT
    qemu_context.cpp
    qemu_init_hooks.cpp
    qemu_machine.cpp
    qemu_disk.cpp
    qemu_console.cpp
    qemu_event_dispatcher.cpp
    qemu_qmp.cpp
)
target_include_directories(qvadis_integration PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/qvadis
    ${CMAKE_CURRENT_SOURCE_DIR}
)
