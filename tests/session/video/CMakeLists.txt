ihs_add_test(partial_frames test_partial_frames.c)
ihs_add_test(decoder_loss test_decoder_loss.c)
target_link_libraries(ihstest_decoder_loss PRIVATE ihs-test-session)
