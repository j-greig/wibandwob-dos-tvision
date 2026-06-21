# Vintage Turbo Vision demos — ported from sigala.it/sergio/tvision/resources
#
# Can build standalone (no tvterm/micropolis submodules needed):
#   cmake -B build/demos -S app/demos && cmake --build build/demos
#
# Or as part of the full project (add_subdirectory from app/CMakeLists.txt)

cmake_minimum_required(VERSION 3.10)

# When built standalone, set up tvision ourselves
if(NOT TARGET tvision)
    project(tv-demos CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(TV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/tvision tvision)
endif()

file(GLOB DEMO_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
foreach(demo_src ${DEMO_SOURCES})
    get_filename_component(demo_name ${demo_src} NAME_WE)
    add_executable(demo_${demo_name} ${demo_src})
    target_link_libraries(demo_${demo_name} tvision)
endforeach()
