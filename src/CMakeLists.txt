target_sources(ihslib PRIVATE
        base.c
        crc32.c
        crc32c.c
        ihs_timer.c
        ihs_ip.c
        ihs_buffer.c
        ihs_queue.c
        ihs_arraylist.c
        ihs_enumeration.c
        ihs_enumeration_ll.c
        ihs_enumeration_array.c)

add_subdirectory(client)
add_subdirectory(session)