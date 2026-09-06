# FindQEMU.cmake
# Locates QEMU include headers and libraries or builds

include(FindPackageHandleStandardArgs)

set(QEMU_ROOT "" CACHE PATH "Path to QEMU installation or build tree")

find_path(QEMU_INCLUDE_DIR
    NAMES qemu/osdep.h qemu-main.h
    PATHS
        ${QEMU_ROOT}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../qemu/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qemu/include
        /opt/qemu/include
        /usr/include
        /usr/local/include
)

find_library(QEMU_LIBRARY
    NAMES qemu qemuruntime qemu-system-x86_64
    PATHS
        ${QEMU_ROOT}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/../qemu/build
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qemu/build
        /opt/qemu/lib
        /usr/lib
        /usr/local/lib
)

find_package_handle_standard_args(QEMU
    DEFAULT_MSG
    QEMU_INCLUDE_DIR
)

if(QEMU_FOUND)
    set(QEMU_INCLUDE_DIRS ${QEMU_INCLUDE_DIR})
    set(QEMU_LIBRARIES ${QEMU_LIBRARY})
    
    if(NOT TARGET QEMU::QEMU)
        add_library(QEMU::QEMU UNKNOWN IMPORTED)
        set_target_properties(QEMU::QEMU PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${QEMU_INCLUDE_DIRS}"
        )
        if(QEMU_LIBRARY)
            set_target_properties(QEMU::QEMU PROPERTIES
                IMPORTED_LOCATION "${QEMU_LIBRARY}"
            )
        endif()
    endif()
endif()
