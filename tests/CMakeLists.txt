find_package(Criterion REQUIRED)

add_executable(ccos_tests
        ${CMAKE_CURRENT_LIST_DIR}/api_test.c
        ${CMAKE_CURRENT_LIST_DIR}/format_test.c
        ${CMAKE_CURRENT_LIST_DIR}/datetime_stub.c
        )

target_link_libraries(ccos_tests PRIVATE ccos_api Criterion::Criterion)

add_test(NAME ccos_tests COMMAND ccos_tests WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
