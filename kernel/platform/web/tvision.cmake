# Patched Turbo Vision for the browser build.
#
# vendor/tvision is a git submodule and is left untouched. At configure time its
# sources are copied into the build tree and the patches in patches/tvision/ are
# applied to the copy in order. The native build keeps using the submodule as is.

set(WWDOS_TV_SRC ${CMAKE_SOURCE_DIR}/../vendor/tvision)
set(WWDOS_TV_PATCHES_DIR ${CMAKE_CURRENT_LIST_DIR}/patches/tvision)
set(WWDOS_TV ${CMAKE_BINARY_DIR}/tvision-src)

file(GLOB WWDOS_TV_PATCHES ${WWDOS_TV_PATCHES_DIR}/*.patch)
list(SORT WWDOS_TV_PATCHES)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${WWDOS_TV_PATCHES})

if(NOT EXISTS ${WWDOS_TV_SRC}/include/tvision/tv.h)
    message(FATAL_ERROR "vendor/tvision is empty: git submodule update --init vendor/tvision")
endif()

file(REMOVE_RECURSE ${WWDOS_TV})
file(COPY ${WWDOS_TV_SRC}/include ${WWDOS_TV_SRC}/source DESTINATION ${WWDOS_TV})
file(COPY ${WWDOS_TV_SRC}/hello.cpp DESTINATION ${WWDOS_TV})
find_package(Git REQUIRED)
# The copy gets its own repository so git apply resolves paths against it, not against
# the enclosing checkout (git apply skips paths outside the current repository's subdirectory).
execute_process(COMMAND ${GIT_EXECUTABLE} init -q WORKING_DIRECTORY ${WWDOS_TV})
foreach(p ${WWDOS_TV_PATCHES})
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply --whitespace=nowarn ${p}
        WORKING_DIRECTORY ${WWDOS_TV}
        RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "tvision patch failed: ${p}")
    endif()
endforeach()

file(GLOB_RECURSE WWDOS_TV_SOURCES ${WWDOS_TV}/source/*/*.cpp)
list(REMOVE_ITEM WWDOS_TV_SOURCES ${WWDOS_TV}/source/tvision/geninc.cpp)
add_library(tvision STATIC ${WWDOS_TV_SOURCES})
target_include_directories(tvision PUBLIC ${WWDOS_TV}/include)
target_include_directories(tvision PRIVATE
    ${WWDOS_TV}/include/tvision
    ${WWDOS_TV}/include/tvision/compat/borland
    ${WWDOS_TV}/include/tvision/compat/windows)
target_compile_definitions(tvision PRIVATE TVISION_NO_STL)
target_compile_options(tvision PRIVATE -Wno-unused-function -Wno-deprecated-declarations)
