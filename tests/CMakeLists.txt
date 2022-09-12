set(TEST_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../src)

function(ihs_add_test NAME SOURCES)
    set(TEST_EXE "ihstest_${NAME}")
    add_executable(${TEST_EXE} ${SOURCES})
    add_test(${NAME} ${TEST_EXE})
    target_include_directories(${TEST_EXE} PRIVATE ${TEST_INCLUDES})
    target_link_libraries(${TEST_EXE} PRIVATE ihslib)
endfunction()

add_subdirectory(session)