cmake_minimum_required(VERSION 3.4.1)

project(ccos_disk_tool)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${SOURCE_DIR})

add_library(ccos_api STATIC
        ${SOURCE_DIR}/string_utils.h
        ${SOURCE_DIR}/string_utils.c
        ${SOURCE_DIR}/common.h
        ${SOURCE_DIR}/common.c
        ${SOURCE_DIR}/ccos_image.h
        ${SOURCE_DIR}/ccos_image.c
        ${SOURCE_DIR}/ccos_private.c
        ${SOURCE_DIR}/ccos_private.h
        )

add_executable(ccos_disk_tool
        ${SOURCE_DIR}/wrapper.h
        ${SOURCE_DIR}/wrapper.c
        ${SOURCE_DIR}/main.c
        )

TARGET_LINK_LIBRARIES(ccos_disk_tool ccos_api)

project(api_test)

add_executable(api_test
        ${SOURCE_DIR}/api_test.c
        )

TARGET_LINK_LIBRARIES(api_test ccos_api)
