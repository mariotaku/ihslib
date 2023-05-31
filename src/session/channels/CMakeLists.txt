target_sources(ihslib PRIVATE
        channel.c
        ch_discovery.c
        ch_control.c
        ch_control_authentication.c
        ch_control_negotiation.c
        ch_control_keepalive.c
        ch_control_audio.c
        ch_control_video.c
        ch_data.c
        ch_data_audio.c
        ch_stats.c)

add_subdirectory(control)
add_subdirectory(video)