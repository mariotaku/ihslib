ihs_add_test(packet_conn_read_write packet_conn_read_write.c)
ihs_add_test(packet_ping_resp packet_ping_resp.c)
ihs_add_test(packet_generation packet_generation.c)
ihs_add_test(ip_address test_ip_address.c)

ihs_add_test(window test_window.c)
target_link_libraries(ihstest_window PRIVATE ihs-test-session)

ihs_add_test(timer test_timer.c)
ihs_add_test(disconnect_destroy test_disconnect_destroy.c)
target_link_libraries(ihstest_disconnect_destroy PRIVATE ihs-test-session)

ihs_add_test(activity test_activity.c)
target_link_libraries(ihstest_activity PRIVATE ihs-test-session)

ihs_add_test(input_mark test_input_mark.c)
target_link_libraries(ihstest_input_mark PRIVATE ihs-test-session)

ihs_add_test(input_mouse test_input_mouse.c)
target_link_libraries(ihstest_input_mouse PRIVATE ihs-test-session)

ihs_add_test(control_hid test_control_hid.c)
target_link_libraries(ihstest_control_hid PRIVATE ihs-test-session)

add_subdirectory(video)
ihs_add_test(retransmission_twin retransmission_twin.c)
target_link_libraries("${IHSTEST_TARGET}" PRIVATE ihs-test-session)
