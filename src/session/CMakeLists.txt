target_sources(ihslib PRIVATE
        session.c
        packet.c
        frame.c
        window.c
        frame_crypto.c
        frame_stats.c
        callbacks.c
        retransmission.c)
add_subdirectory(channels)