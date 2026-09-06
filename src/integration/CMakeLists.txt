add_library(qvadis_integration OBJECT
    qemu_context.cpp
    qemu_init_hooks.cpp
)
target_include_directories(qvadis_integration PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/qvadis
    ${CMAKE_CURRENT_SOURCE_DIR}
)
