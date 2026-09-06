cmake_minimum_required(VERSION 3.20)
project(qvadis VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find optional QEMU dependency
find_package(QEMU QUIET)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qvadis
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add modular subdirectories
add_subdirectory(src)

# Core shared library
add_library(qemuruntime SHARED
    $<TARGET_OBJECTS:qvadis_integration>
    $<TARGET_OBJECTS:qvadis_c_api>
    $<TARGET_OBJECTS:qvadis_cpp_wrapper>
)

target_compile_definitions(qemuruntime PRIVATE QEMU_RUNTIME_EXPORTS)

target_include_directories(qemuruntime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/qvadis>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(qemuruntime PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    OUTPUT_NAME "qemuruntime"
)

# Core static library
add_library(qemuruntime_static STATIC
    $<TARGET_OBJECTS:qvadis_integration>
    $<TARGET_OBJECTS:qvadis_c_api>
    $<TARGET_OBJECTS:qvadis_cpp_wrapper>
)
target_compile_definitions(qemuruntime_static PUBLIC QEMU_RUNTIME_STATIC)
target_include_directories(qemuruntime_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/qvadis>
    $<INSTALL_INTERFACE:include>
)

# Testing
enable_testing()

add_executable(test_qvadis tests/test_qvadis.cpp)
target_link_libraries(test_qvadis PRIVATE qemuruntime_static)
add_test(NAME test_qvadis COMMAND test_qvadis)

add_executable(test_phase_3_4 tests/phase_3_4_tests/test_phase_3_4.cpp)
target_link_libraries(test_phase_3_4 PRIVATE qemuruntime_static)
add_test(NAME test_phase_3_4 COMMAND test_phase_3_4)

add_executable(test_phase_5 tests/phase_5_tests/test_phase_5.cpp)
target_link_libraries(test_phase_5 PRIVATE qemuruntime_static)
add_test(NAME test_phase_5 COMMAND test_phase_5)

add_executable(test_phase_6 tests/phase_6_tests/test_phase_6.cpp)
target_link_libraries(test_phase_6 PRIVATE qemuruntime_static)
add_test(NAME test_phase_6 COMMAND test_phase_6)

add_executable(test_phase_7 tests/phase_7_tests/test_phase_7.cpp)
target_link_libraries(test_phase_7 PRIVATE qemuruntime_static)
add_test(NAME test_phase_7 COMMAND test_phase_7)

add_executable(test_phase_8 tests/phase_8_tests/test_phase_8.cpp)
target_link_libraries(test_phase_8 PRIVATE qemuruntime_static)
add_test(NAME test_phase_8 COMMAND test_phase_8)

add_executable(test_phase_10 tests/phase_10_tests/test_phase_10.cpp)
target_link_libraries(test_phase_10 PRIVATE qemuruntime_static)
add_test(NAME test_phase_10 COMMAND test_phase_10)

# Install rules
include(GNUInstallDirs)
install(TARGETS qemuruntime qemuruntime_static
    EXPORT QemuRuntimeTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/qvadis DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
