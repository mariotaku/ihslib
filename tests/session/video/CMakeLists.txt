ihs_add_test(partial_frames test_partial_frames.c)

ihs_add_test(video_delivery test_video_delivery.c)
target_link_libraries(ihstest_video_delivery PRIVATE ihs-test-session)
