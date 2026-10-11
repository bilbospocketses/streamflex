# The built Windows exe carries SDL3's Vulkan fix for a transparent window (spec section 9, test 13):
# its error string is in vcpkg-overlays/sdl3's patched SDL_render_vulkan.c and in no SDL release. No
# CI runner has a GPU to drive the fix itself, so the string is the proof the overlay port was built.
# Run as cmake -DEXE=<file> -P check_vulkan_fix.cmake
if (NOT EXISTS "${EXE}")
  message(FATAL_ERROR "There is no file at ${EXE}")
endif ()
file(STRINGS "${EXE}" found LIMIT_COUNT 1
     REGEX "this surface offers no composite alpha mode for a transparent window")
if (NOT found)
  message(FATAL_ERROR "${EXE} lacks the Vulkan fix's error string: the sdl3 overlay port was not built")
endif ()
message(STATUS "${EXE} carries the Vulkan fix: ${found}")
