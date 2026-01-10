cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(ccos_disk_tool)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${SOURCE_DIR})

option(CCOS_ENABLE_TESTS "Build and run tests (Criterion)" OFF)

add_library(ccos_api STATIC
        ${SOURCE_DIR}/string_utils.h
        ${SOURCE_DIR}/string_utils.c
        ${SOURCE_DIR}/common.h
        ${SOURCE_DIR}/common.c
        ${SOURCE_DIR}/ccos_image.h
        ${SOURCE_DIR}/ccos_image.c
        ${SOURCE_DIR}/ccos_private.h
        ${SOURCE_DIR}/ccos_private.c
        ${SOURCE_DIR}/ccos_structure.h
        ${SOURCE_DIR}/ccos_structure.c
        )

add_executable(ccos_disk_tool
        ${SOURCE_DIR}/wrapper.h
        ${SOURCE_DIR}/wrapper.c
        ${SOURCE_DIR}/main.c
        )

TARGET_LINK_LIBRARIES(ccos_disk_tool ccos_api)

if(CCOS_ENABLE_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
