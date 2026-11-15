# WibWob-DOS desktop for the browser. Added by kernel/CMakeLists.txt (WWDOS_WEB_APP).
# The source list follows the wwdos target in app/CMakeLists.txt. Differences:
#   - tvterm_view.cpp compiles to nothing; tvterm_stub.cpp is a placeholder window (no PTY).
#   - micropolis_stub.cpp replaces the Micropolis engine (not built for the browser).
#   - no tvterm-core, libvterm or OpenSSL; api_ipc.cpp is compiled but the server never starts.
set(APP ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(WWDOS_WEB_SOURCES
    wwdos_app.cpp
    test_pattern.cpp gradient.cpp glitch_engine.cpp frame_capture.cpp api_ipc.cpp
    command_registry.cpp resident_input.cpp wibwob_background.cpp window_type_registry.cpp theme_manager.cpp
    ega_theme.cpp wibwob_view.cpp wibwob_engine.cpp room_chat_view.cpp
    llm/base/auth_config.cpp llm/base/llm_config.cpp llm/base/llm_provider_factory.cpp
    llm/base/tool_executor.cpp llm/providers/claude_code_sdk_provider.cpp
    llm/providers/anthropic_api_provider.cpp llm/tools/time_tools.cpp llm/tools/tui_tools.cpp
    ascii_grid_view.cpp ascii_image_view.cpp frame_file_player_view.cpp animated_blocks_view.cpp
    animated_gradient_view.cpp animated_score_view.cpp animated_ascii_view.cpp game_of_life_view.cpp
    quadra_view.cpp snake_view.cpp tr808_engine.cpp tr808_audio.cpp tr808_view.cpp
    rogue_view.cpp deep_signal_view.cpp app_launcher_view.cpp
    ascii_gallery_view.cpp generative_verse_view.cpp generative_orbit_view.cpp
    generative_mycelium_view.cpp generative_torus_view.cpp generative_monster_portal_view.cpp
    generative_cube_view.cpp generative_monster_verse_view.cpp generative_monster_cam_view.cpp
    text_editor_view.cpp browser_view.cpp text_wrap.cpp transparent_text_view.cpp ansi_view.cpp
    wibwob_scroll_test.cpp contour_map_view.cpp generative_lab_view.cpp scramble_view.cpp
    scramble_engine.cpp
    paint/paint_canvas.cpp paint/paint_palette.cpp paint/paint_status.cpp paint/paint_tools.cpp
    paint/paint_wwp_codec.cpp paint/paint_window.cpp windows/frame_animation_window.cpp
    paint/figlet_stamp_dialog.cpp
    micropolis_ascii_view.cpp micropolis_tool_palette.cpp
    tvterm_view.cpp figlet_utils.cpp figlet_text_view.cpp backrooms_tv_view.cpp
)
list(TRANSFORM WWDOS_WEB_SOURCES PREPEND ${APP}/)
add_executable(wwdos
    ${WWDOS_WEB_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/tvterm_stub.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/micropolis_stub.cpp)
target_include_directories(wwdos PRIVATE ${APP})
target_compile_definitions(wwdos PRIVATE WWDOS_WASM=1)
target_compile_options(wwdos PRIVATE -Wno-deprecated-declarations -fexceptions)
target_link_libraries(wwdos PRIVATE tvision)
# The primer library is embedded in the wasm data segment, so the page needs no fetch.
target_link_options(wwdos PRIVATE ${WWDOS_WEB_LINK_FLAGS} -fexceptions
    "SHELL:--embed-file ${APP}/../modules/wibwob-2026/primers@/modules/wibwob-2026/primers")
set_target_properties(wwdos PROPERTIES SUFFIX ".js" RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
