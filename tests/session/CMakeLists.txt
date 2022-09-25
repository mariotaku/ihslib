add_library(ihs-test-session STATIC test_session.c)
target_include_directories(ihs-test-session PRIVATE ${TEST_INCLUDES})

ihs_add_test(packet_conn_read_write packet_conn_read_write.c)
ihs_add_test(packet_ping_resp packet_ping_resp.c)