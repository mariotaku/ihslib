add_library(ihs-test-session STATIC test_session.c)
target_include_directories(ihs-test-session PRIVATE ${TEST_INCLUDES})

ihs_add_test(packet_conn_read_write packet_conn_read_write.c)
ihs_add_test(packet_ping_resp packet_ping_resp.c)
ihs_add_test(packet_generation packet_generation.c)
ihs_add_test(ip_address test_ip_address.c)

ihs_add_test(timer test_timer.c)