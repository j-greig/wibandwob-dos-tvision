# The terminal-film player: a small desktop (app/film) that runs the stage driver (app/stage).
# Two ways in, one source list:
#   - browser: kernel/CMakeLists.txt adds this directory when WWDOS_WEB_FILM is on (the default)
#     and kernel/build-web.sh bundles build-web/film.js into dist-web/film.html.
#   - terminal: cmake -B build-stage -S app/film && cmake --build build-stage -j8
#     builds build-stage/wwdos-stage with Turbo Vision's ncurses platform (docs/stage-driver.md).
# The source order, defines and link flags follow the wwdos-min target of the on-chain repository
# (j-greig/wibandwob-dos-onchain, CMakeLists.txt): keep them in step, the film wasm is compared
# byte for byte against the on-chain build.
cmake_minimum_required(VERSION 3.13)

set(FILM_APP ${CMAKE_CURRENT_LIST_DIR}/..)
set(FILM_SOURCES
    ${FILM_APP}/film/minimal_app.cpp
    ${FILM_APP}/stage/stage.cpp
    ${FILM_APP}/stage/mirror_view.cpp
    ${FILM_APP}/stage/picture.cpp
    ${FILM_APP}/windows/frame_animation_window.cpp
    ${FILM_APP}/frame_file_player_view.cpp
    ${FILM_APP}/gradient.cpp
    ${FILM_APP}/ega_theme.cpp
    ${FILM_APP}/wibwob_background.cpp
)
# The film host includes the driver's headers by bare name, and the driver includes the app's views.
set(FILM_INCLUDES ${FILM_APP} ${FILM_APP}/stage ${FILM_APP}/film)

if(EMSCRIPTEN)
    # ---- browser target: film.js (added by kernel/CMakeLists.txt, which defines tvision and the link flags) ----
    add_executable(film ${FILM_SOURCES})
    target_include_directories(film PRIVATE ${FILM_INCLUDES})
    target_compile_definitions(film PRIVATE WWDOS_WASM=1 WWDOS_MINIMAL=1)
    target_compile_options(film PRIVATE -Wno-deprecated-declarations)
    target_link_libraries(film PRIVATE tvision)
    # FS is exported so a page can write /boot/scene.txt and its primers in preRun (the on-chain tools/scene.py).
    target_link_options(film PRIVATE ${WWDOS_WEB_LINK_FLAGS} -sEXPORTED_RUNTIME_METHODS=FS,ccall,UTF8ToString
        "SHELL:--embed-file ${CMAKE_CURRENT_LIST_DIR}/boot@/boot")
    set_target_properties(film PROPERTIES SUFFIX ".js" RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    return()
endif()

# ---- native terminal target: wwdos-stage ----
project(wwdos-stage CXX C)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
# The same patched Turbo Vision as the browser build (kernel/platform/web/tvision.cmake copies the
# submodule and applies the patches), with the ncurses platform and without the browser console.
include(${CMAKE_CURRENT_LIST_DIR}/../../kernel/platform/web/tvision.cmake)
list(REMOVE_ITEM WWDOS_TV_SOURCES ${WWDOS_TV}/source/platform/wasmcon.cpp)
add_library(tvision-native STATIC ${WWDOS_TV_SOURCES})
target_include_directories(tvision-native PUBLIC ${WWDOS_TV}/include)
target_include_directories(tvision-native PRIVATE
    ${WWDOS_TV}/include/tvision ${WWDOS_TV}/include/tvision/compat/borland
    ${WWDOS_TV}/include/tvision/compat/windows ${WWDOS_TV}/include/tvision/compat/malloc)
# Homebrew's ncursesw first (wide characters, 6.x); the system's otherwise.
find_library(NCURSESW NAMES ncursesw ncurses HINTS /opt/homebrew/opt/ncurses/lib /usr/local/opt/ncurses/lib)
find_path(NCURSESW_INCLUDE ncursesw/ncurses.h HINTS /opt/homebrew/opt/ncurses/include /usr/local/opt/ncurses/include)
if(NCURSESW_INCLUDE)
    target_include_directories(tvision-native PRIVATE ${NCURSESW_INCLUDE}/ncursesw ${NCURSESW_INCLUDE})
endif()
target_compile_definitions(tvision-native PRIVATE TVISION_NO_STL HAVE_NCURSES)
target_compile_options(tvision-native PRIVATE -Wno-unused-function -Wno-deprecated-declarations)
target_link_libraries(tvision-native PUBLIC ${NCURSESW})

add_executable(wwdos-stage ${FILM_SOURCES})
target_include_directories(wwdos-stage PRIVATE ${FILM_INCLUDES})
target_compile_definitions(wwdos-stage PRIVATE WWDOS_MINIMAL=1)
target_compile_options(wwdos-stage PRIVATE -Wno-deprecated-declarations)
target_link_libraries(wwdos-stage PRIVATE tvision-native)
