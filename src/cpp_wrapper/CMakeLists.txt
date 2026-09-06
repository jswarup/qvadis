add_library(qvadis_cpp_wrapper OBJECT
    QemuRuntime.cpp
)
target_include_directories(qvadis_cpp_wrapper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/qvadis
)
