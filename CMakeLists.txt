cmake_minimum_required(VERSION 3.20)
project(Tonkio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Shared gameplay code (header-only for now), used by both client and server.
add_library(tonk_core INTERFACE)
target_include_directories(tonk_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(EMSCRIPTEN)
  # Browser client: cmake via `emcmake cmake -B build-web && cmake --build build-web`
  add_executable(tonk_client client/main.cpp)
  target_link_libraries(tonk_client PRIVATE tonk_core)
  target_link_options(tonk_client PRIVATE
    -sUSE_WEBGL2=1
    -sMIN_WEBGL_VERSION=2
    -sMAX_WEBGL_VERSION=2
    -sALLOW_MEMORY_GROWTH=1
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
    -sEXPORTED_FUNCTIONS=_main,_tonk_debug_give,_tonk_debug_goto_crate,_tonk_debug_step,_tonk_debug_info,_tonk_restart,_tonk_set_timescale,_tonk_set_god,_tonk_set_debug_bots,_tonk_set_respawn,_tonk_add_time,_tonk_skip_ring,_tonk_spectate,_tonk_teleport_edge,_tonk_play,_tonk_to_menu,_tonk_set_look,_tonk_clear_bot_looks,_tonk_look_slot,_tonk_debug_relic,_tonk_debug_goto,_tonk_bg_tick)
  set_target_properties(tonk_client PROPERTIES
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/web)
else()
  # Native: authoritative game server + tests.
  add_executable(tonk_server server/main.cpp)
  target_link_libraries(tonk_server PRIVATE tonk_core)

  enable_testing()
  add_executable(tonk_tests tests/core_tests.cpp)
  target_link_libraries(tonk_tests PRIVATE tonk_core)
  add_test(NAME core COMMAND tonk_tests)
endif()
