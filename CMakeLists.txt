cmake_minimum_required(VERSION 3.4.1)

project(ccos_disk_tool)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${SOURCE_DIR})

add_executable(ccos_disk_tool
        ${SOURCE_DIR}/main.c
        ${SOURCE_DIR}/string_utils.h
        ${SOURCE_DIR}/string_utils.c
        ${SOURCE_DIR}/wrapper.h
        ${SOURCE_DIR}/wrapper.c
        ${SOURCE_DIR}/common.h
        ${SOURCE_DIR}/common.c
        ${SOURCE_DIR}/ccos_image.h
        ${SOURCE_DIR}/ccos_image.c)
