# The Phase 0 probe kit (design/plans/2026-10-27-overlay-phase0.md). Not part of StreamFlex's build.
cmake_minimum_required(VERSION 3.18)
project(cptk_overlay_phase0 LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
if (MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif ()

add_library(probe_common STATIC src/common.c)
target_include_directories(probe_common PUBLIC src)

add_executable(probe_host src/probe_host.c)
target_link_libraries(probe_host PRIVATE probe_common)
