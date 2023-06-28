find_package(PkgConfig REQUIRED)

pkg_check_modules(HIDAPI_HIDRAW hidapi-hidraw)

if (NOT HIDAPI_HIDRAW_FOUND)
    return()
endif ()

add_library(ihslib-hid-hidapi STATIC)
target_link_libraries(ihslib-hid-hidapi PUBLIC ihslib PRIVATE ihs-protobuf ${HIDAPI_HIDRAW_LIBRARIES})
target_include_directories(ihslib-hid-hidapi PUBLIC include ${GEN_INCLUDE_DIR}
        SYSTEM PUBLIC ${HIDAPI_HIDRAW_INCLUDE_DIRS} PRIVATE ../..)

add_subdirectory(src)