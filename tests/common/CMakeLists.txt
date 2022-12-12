add_library(ihs-test-session STATIC test_session.c)
target_include_directories(ihs-test-session PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${TEST_INCLUDES})
