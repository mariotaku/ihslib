set(TEST_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../src)

function(ihs_add_test NAME SOURCES)
    set(TEST_EXE "ihstest_${NAME}")
    add_executable(${TEST_EXE} ${SOURCES})
    add_test(${NAME} ${TEST_EXE})
    target_include_directories(${TEST_EXE} PRIVATE ${TEST_INCLUDES})
    target_link_libraries(${TEST_EXE} PRIVATE ihslib)
endfunction()

function(ihs_test_link_libraries NAME LIBRARIES)
    set(TEST_EXE "ihstest_${NAME}")
    target_link_libraries(${TEST_EXE} PRIVATE ${LIBRARIES})
endfunction()

ihs_add_test(arraylist test_arraylist.c)
ihs_add_test(enumeration test_enumeration.c)

add_subdirectory(hid)
add_subdirectory(session)
