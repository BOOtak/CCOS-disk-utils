cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(ccos_api LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(ccos_api STATIC
        ccos_disk.h
        ccos_error.h
        ccos_error.c
        ccos_image.h
        ccos_image.c
        ccos_boot_data.h
        ccos_format.h
        ccos_format.c
        ccos_private.h
        ccos_private.c
        ccos_structure.h
        ccos_structure.c
        ccos_string.h
        ccos_string.c
        )

target_include_directories(ccos_api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

option(CCOS_ENABLE_TESTS "Build and run tests (Criterion)" OFF)

if(CCOS_ENABLE_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

add_subdirectory(tool)
