cmake_minimum_required(VERSION 3.10)

project(ccos_disk_tool)

add_executable(ccos_disk_tool
        main.c
        wrapper.h
        wrapper.c
        string_utils.h
        string_utils.c
        common.h
        common.c
        )

target_link_libraries(ccos_disk_tool PRIVATE ccos_api)
