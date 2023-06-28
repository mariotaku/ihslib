string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$" SDL_SEMVER "${IHSLIB_SDL_TARGETVERSION}")
if (SDL_SEMVER)
    set(IHSLIB_SDL_TARGET_MAJOR ${CMAKE_MATCH_1})
    set(IHSLIB_SDL_TARGET_MINOR ${CMAKE_MATCH_2})
    set(IHSLIB_SDL_TARGET_PATCH ${CMAKE_MATCH_3})
else()
    set(IHSLIB_SDL_TARGET_MAJOR SDL_MAJOR_VERSION)
    set(IHSLIB_SDL_TARGET_MINOR SDL_MINOR_VERSION)
    set(IHSLIB_SDL_TARGET_PATCH SDL_PATCHLEVEL)
endif ()

set(GEN_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_include)

add_library(ihslib-hid-sdl STATIC)
target_link_libraries(ihslib-hid-sdl PUBLIC ihslib PRIVATE ihs-protobuf ${SDL2_LIBRARIES})
target_include_directories(ihslib-hid-sdl PUBLIC include ${GEN_INCLUDE_DIR} SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS} PRIVATE ../..)

add_subdirectory(src)