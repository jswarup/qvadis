add_library(qvadis_c_api OBJECT
    qemu_runtime.cpp
)
target_include_directories(qvadis_c_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/qvadis
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration
)
target_compile_definitions(qvadis_c_api PRIVATE QEMU_RUNTIME_EXPORTS)
