ihs_add_test(packet_conn_read_write packet_conn_read_write.c)
ihs_add_test(packet_ping_resp packet_ping_resp.c)
ihs_add_test(packet_generation packet_generation.c)
ihs_add_test(ip_address test_ip_address.c)

ihs_add_test(timer test_timer.c)
ihs_add_test(disconnect_destroy test_disconnect_destroy.c)
target_link_libraries(ihstest_disconnect_destroy PRIVATE ihs-test-session)

ihs_add_test(video_control test_video_control.c)
target_link_libraries(ihstest_video_control PRIVATE ihs-test-session)

ihs_add_test(control_keyboard test_control_keyboard.c)
target_link_libraries(ihstest_control_keyboard PRIVATE ihs-test-session)

ihs_add_test(frame_stats test_frame_stats.c)

add_subdirectory(video)