# Browser build (Emscripten). Configure with emcmake; see kernel/build-web.sh.
# The native build is the top-level CMakeLists.txt and does not read this file.
cmake_minimum_required(VERSION 3.13)
project(wwdos-web CXX C)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "kernel/ is the browser build: run kernel/build-web.sh (emcmake)")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")

option(WWDOS_WEB_APP "Also build the WibWob-DOS desktop (app/web), not only the Turbo Vision demo" ON)
option(WWDOS_SINGLE_FILE "Inline the wasm into the .js as base64 (needed for one-file pages)" ON)

include(platform/web/tvision.cmake)

# Link flags shared by every page.
# ASYNCIFY lets Turbo Vision's blocking event wait unwind to the browser, so nested
# modal loops (execView: menus, dialogs) work without restructuring.
set(WWDOS_WEB_LINK_FLAGS
    -sASYNCIFY=1
    -sASYNCIFY_STACK_SIZE=262144
    -sSTACK_SIZE=1048576
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=33554432
    -sMODULARIZE=1
    -sEXPORT_NAME=WWDOS
    -sENVIRONMENT=web
    -sEXIT_RUNTIME=0
    -sINVOKE_RUN=1
    -sSINGLE_FILE=$<BOOL:${WWDOS_SINGLE_FILE}>
    -Os)

# Turbo Vision's own hello demo: the smallest page that proves the platform layer.
add_executable(hello ${WWDOS_TV}/hello.cpp)
target_link_libraries(hello PRIVATE tvision)
target_link_options(hello PRIVATE ${WWDOS_WEB_LINK_FLAGS})
set_target_properties(hello PROPERTIES SUFFIX ".js")

if(WWDOS_WEB_APP)
    add_subdirectory(${CMAKE_SOURCE_DIR}/../app/web ${CMAKE_BINARY_DIR}/app-web)
endif()
