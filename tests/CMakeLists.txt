# Unit tests for the pure layout module. layout.c must stay free of SDL, so this links it alone.
add_executable(test_layout test_layout.c "${PROJECT_SOURCE_DIR}/src/layout.c")
target_include_directories(test_layout PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME layout COMMAND test_layout)

# Unit tests for the pure UTF-8 helpers (title truncation), likewise linked on their own
add_executable(test_utf8 test_utf8.c "${PROJECT_SOURCE_DIR}/src/utf8.c")
target_include_directories(test_utf8 PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME utf8 COMMAND test_utf8)

# Unit tests for keeping images off the chroma key (a pure module, linked on its own)
add_executable(test_chroma test_chroma.c "${PROJECT_SOURCE_DIR}/src/chroma.c")
target_include_directories(test_chroma PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME chroma COMMAND test_chroma)

# Unit tests for the note the launcher logs after SDL chooses its video driver (a pure module)
add_executable(test_videodriver test_videodriver.c "${PROJECT_SOURCE_DIR}/src/videodriver.c")
target_include_directories(test_videodriver PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME videodriver COMMAND test_videodriver)

# Unit tests for the gamepad battery glyph's rules (pure; the glyph's color is kept off the chroma
# key through chroma.c)
add_executable(test_padpower test_padpower.c "${PROJECT_SOURCE_DIR}/src/padpower.c" "${PROJECT_SOURCE_DIR}/src/chroma.c")
target_include_directories(test_padpower PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME padpower COMMAND test_padpower)

# fileio.c finds the Windows places (Pictures, drives) through the shell
function(link_fileio target)
  if (WIN32)
    target_link_libraries(${target} shell32 ole32 uuid)
  endif ()
endfunction()

# Unit tests for UTF-8 file access (non-ASCII names, the replace and its retry on Windows)
add_executable(test_fileio test_fileio.c "${PROJECT_SOURCE_DIR}/src/fileio.c" "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_fileio PRIVATE "${PROJECT_SOURCE_DIR}/src")
link_fileio(test_fileio)
if (UNIX)
  # The test checks that each thread keeps its own reason
  find_package(Threads REQUIRED)
  target_link_libraries(test_fileio Threads::Threads)
endif ()
add_test(NAME fileio COMMAND test_fileio)

# Unit tests for editing config.ini line by line; the shipped sample config must survive a round trip
add_executable(test_inidoc test_inidoc.c "${PROJECT_SOURCE_DIR}/src/inidoc.c" "${PROJECT_SOURCE_DIR}/src/fileio.c"
  "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_inidoc PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(test_inidoc PRIVATE SAMPLE_CONFIG="${PROJECT_BINARY_DIR}/config.ini")
link_fileio(test_inidoc)
add_test(NAME inidoc COMMAND test_inidoc)

# Unit tests for the settings screen's save: fresh read, edits, backup, replace and the Linux fallback
add_executable(test_config_save test_config_save.c "${PROJECT_SOURCE_DIR}/src/config_save.c"
  "${PROJECT_SOURCE_DIR}/src/inidoc.c" "${PROJECT_SOURCE_DIR}/src/fileio.c" "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_config_save PRIVATE "${PROJECT_SOURCE_DIR}/src")
link_fileio(test_config_save)
add_test(NAME config_save COMMAND test_config_save)

# Unit tests for the settings table and the settings screen's pages; settings.c reads counts and
# sizes through layout.c's parsers, paths through fileio.c's helpers, the key and gamepad bindings
# through bindings.c's model (whose lines the tests parse with inidoc.c), and names a color through
# colorpick.c's swatches (whose contrast math needs libm)
add_executable(test_settings test_settings.c "${PROJECT_SOURCE_DIR}/src/settings.c" "${PROJECT_SOURCE_DIR}/src/layout.c"
  "${PROJECT_SOURCE_DIR}/src/fileio.c" "${PROJECT_SOURCE_DIR}/src/alloc.c" "${PROJECT_SOURCE_DIR}/src/bindings.c"
  "${PROJECT_SOURCE_DIR}/src/inidoc.c" "${PROJECT_SOURCE_DIR}/src/colorpick.c")
target_include_directories(test_settings PRIVATE "${PROJECT_SOURCE_DIR}/src")
link_fileio(test_settings)
if (UNIX)
  target_link_libraries(test_settings m)
endif ()
add_test(NAME settings COMMAND test_settings)

# Unit tests for deriving the values the launcher draws with from the configured ones (pure)
add_executable(test_derive test_derive.c "${PROJECT_SOURCE_DIR}/src/derive.c")
target_include_directories(test_derive PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME derive COMMAND test_derive)

# Unit tests for the shared list picker's model (pure)
add_executable(test_listpick test_listpick.c "${PROJECT_SOURCE_DIR}/src/listpick.c" "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_listpick PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME listpick COMMAND test_listpick)

# Unit tests for the color picker's model and the contrast math (pure; the math needs libm)
add_executable(test_colorpick test_colorpick.c "${PROJECT_SOURCE_DIR}/src/colorpick.c")
target_include_directories(test_colorpick PRIVATE "${PROJECT_SOURCE_DIR}/src")
if (UNIX)
  target_link_libraries(test_colorpick m)
endif ()
add_test(NAME colorpick COMMAND test_colorpick)

# Unit tests for grouping font faces into families (pure; names compare through fileio.c's helpers)
add_executable(test_fontlist test_fontlist.c "${PROJECT_SOURCE_DIR}/src/fontlist.c" "${PROJECT_SOURCE_DIR}/src/fileio.c"
  "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_fontlist PRIVATE "${PROJECT_SOURCE_DIR}/src")
link_fileio(test_fontlist)
add_test(NAME fontlist COMMAND test_fontlist)

# Unit tests for the bindings model: lists, the safety floor, confirmation and capture (pure)
add_executable(test_bindings test_bindings.c "${PROJECT_SOURCE_DIR}/src/bindings.c" "${PROJECT_SOURCE_DIR}/src/inidoc.c"
  "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_bindings PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME bindings COMMAND test_bindings)

# Unit tests for the folder browser's model, over a pretend file system
add_executable(test_browser test_browser.c "${PROJECT_SOURCE_DIR}/src/browser.c" "${PROJECT_SOURCE_DIR}/src/fileio.c"
  "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_browser PRIVATE "${PROJECT_SOURCE_DIR}/src")
link_fileio(test_browser)
add_test(NAME browser COMMAND test_browser)

# The allocation-failure proof: every allocation in the pure modules made to fail in turn
# (colorpick.c allocates nothing; settings.c names colors through it)
add_executable(test_alloc test_alloc.c "${PROJECT_SOURCE_DIR}/src/alloc.c" "${PROJECT_SOURCE_DIR}/src/fileio.c"
  "${PROJECT_SOURCE_DIR}/src/inidoc.c" "${PROJECT_SOURCE_DIR}/src/config_save.c" "${PROJECT_SOURCE_DIR}/src/browser.c"
  "${PROJECT_SOURCE_DIR}/src/settings.c" "${PROJECT_SOURCE_DIR}/src/layout.c" "${PROJECT_SOURCE_DIR}/src/listpick.c"
  "${PROJECT_SOURCE_DIR}/src/fontlist.c" "${PROJECT_SOURCE_DIR}/src/bindings.c" "${PROJECT_SOURCE_DIR}/src/colorpick.c")
target_include_directories(test_alloc PRIVATE "${PROJECT_SOURCE_DIR}/src")
link_fileio(test_alloc)
if (UNIX)
  target_link_libraries(test_alloc m)
endif ()
add_test(NAME alloc COMMAND test_alloc)

# A function to link inih the way the launcher does
function(link_inih target)
  if (UNIX)
    target_link_libraries(${target} PkgConfig::INIH)
  else ()
    target_link_libraries(${target} ${INIH})
    target_include_directories(${target} PRIVATE ${INIH_INCLUDE_DIR})
  endif ()
endfunction()

# Unit tests for the icon library's manifest reader. library.c must stay free of SDL, so it links with inih alone.
add_executable(test_library test_library.c "${PROJECT_SOURCE_DIR}/src/library.c" "${PROJECT_SOURCE_DIR}/src/fileio.c"
  "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_library PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(test_library PRIVATE LIBRARY_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/library")
link_inih(test_library)
link_fileio(test_library)
add_test(NAME library COMMAND test_library)

# Every generic icon in the shipped library must parse with nanosvg, the launcher's own SVG parser
add_executable(test_library_svg test_library_svg.c)
target_include_directories(test_library_svg PRIVATE "${PROJECT_SOURCE_DIR}/src")
# Vendored, so a system include, as the launcher has it: compilers do not warn about its code
target_include_directories(test_library_svg SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/src/external")
target_compile_definitions(test_library_svg PRIVATE LIBRARY_ROOT="${PROJECT_SOURCE_DIR}/assets/icons/library")
link_inih(test_library_svg)
if (UNIX)
  target_link_libraries(test_library_svg m)
endif ()
add_test(NAME library_svg COMMAND test_library_svg)

# Unit test 14: the window a background needs and the renderers that may draw it (pure, linked alone)
add_executable(test_winplan test_winplan.c "${PROJECT_SOURCE_DIR}/src/winplan.c")
target_include_directories(test_winplan PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME winplan COMMAND test_winplan)

# The built SDL_ttf shapes text with HarfBuzz (spec section 9, test 5); it needs no window or GPU
add_executable(test_harfbuzz test_harfbuzz.c)
target_link_libraries(test_harfbuzz SDL3_ttf::SDL3_ttf SDL3::SDL3)
add_test(NAME harfbuzz COMMAND test_harfbuzz)

# The Windows exe carries the Vulkan fix of the sdl3 overlay port (spec section 9, test 13). The control
# runs the same script on an exe that links no SDL and must report the string missing, so a script
# that passed anything would show; it passes on that message only, not on any other failure
if (WIN32)
  add_test(NAME vulkan_fix COMMAND ${CMAKE_COMMAND} "-DEXE=$<TARGET_FILE:${EXECUTABLE_TITLE}>"
           -P "${CMAKE_CURRENT_SOURCE_DIR}/check_vulkan_fix.cmake")
  add_test(NAME vulkan_fix_control COMMAND ${CMAKE_COMMAND} "-DEXE=$<TARGET_FILE:test_layout>"
           -P "${CMAKE_CURRENT_SOURCE_DIR}/check_vulkan_fix.cmake")
  # CMake wraps a long error at a space, wherever the exe's path puts the 80th column, so any
  # whitespace may stand between the message's words
  string(REPLACE " " "[ \t\r\n]+" missing_fix "lacks the Vulkan fix's error string")
  set_tests_properties(vulkan_fix_control PROPERTIES PASS_REGULAR_EXPRESSION "${missing_fix}")
endif ()

# Unit tests for the key codes config.ini stores (SDL2's numbering) against the codes keys send on SDL3 (pure)
add_executable(test_keycodes test_keycodes.c "${PROJECT_SOURCE_DIR}/src/keycodes.c")
target_include_directories(test_keycodes PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME keycodes COMMAND test_keycodes)

# 3b's bindings fixtures load to the bindings they held, and their codes read as the same keys on SDL3
add_executable(test_bindings_fixtures test_bindings_fixtures.c "${PROJECT_SOURCE_DIR}/src/bindings.c"
  "${PROJECT_SOURCE_DIR}/src/inidoc.c" "${PROJECT_SOURCE_DIR}/src/fileio.c" "${PROJECT_SOURCE_DIR}/src/alloc.c"
  "${PROJECT_SOURCE_DIR}/src/keycodes.c")
target_include_directories(test_bindings_fixtures PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(test_bindings_fixtures PRIVATE FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/headless/fixtures")
link_fileio(test_bindings_fixtures)
add_test(NAME bindings_fixtures COMMAND test_bindings_fixtures)

# Unit test 3: the built SDL has its Vulkan renderer. It links SDL3 as the launcher does (Task 1.1's
# target name), and needs no GPU, so it runs on Windows too, where a missing sdl3[vulkan] fails it.
add_executable(test_render_drivers test_render_drivers.c)
target_include_directories(test_render_drivers PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(test_render_drivers SDL3::SDL3)
add_test(NAME render_drivers COMMAND test_render_drivers)

# Unit tests for the VSync watch: a renderer that reports VSync but presents faster than the display
# is found by measurement (spec section 3; pure, linked alone)
add_executable(test_vsyncwatch test_vsyncwatch.c "${PROJECT_SOURCE_DIR}/src/vsyncwatch.c")
target_include_directories(test_vsyncwatch PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME vsyncwatch COMMAND test_vsyncwatch)

# Unit test 11: the manifest asks for per-monitor v2, read from the source on every platform
add_test(NAME manifest_dpi COMMAND ${CMAKE_COMMAND} -DMANIFEST=${PROJECT_SOURCE_DIR}/config/${EXECUTABLE_TITLE}.manifest.in
         -P ${CMAKE_CURRENT_SOURCE_DIR}/check_manifest.cmake)

# The job table behind parallel icon decode (spec section 8): SDL's threads and atomics, no image code.
# The test hook that keeps every thread from starting is built into this target alone.
add_executable(test_iconload test_iconload.c "${PROJECT_SOURCE_DIR}/src/iconload.c")
target_include_directories(test_iconload PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(test_iconload PRIVATE STREAMFLEX_TEST_HOOKS)
target_link_libraries(test_iconload SDL3::SDL3)
add_test(NAME iconload COMMAND test_iconload)

# Unit tests 6 and 7's pure half (spec section 9): the fallback fonts' rules, linked alone
add_executable(test_fontrules test_fontrules.c "${PROJECT_SOURCE_DIR}/src/fontrules.c")
target_include_directories(test_fontrules PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME fontrules COMMAND test_fontrules)

# Unit test 7's real half: font sets over SDL_ttf and the bundled fonts. The test stands in for
# fontcand.c's candidates and for the launcher's log, so no system font is read
add_executable(test_fontset test_fontset.c "${PROJECT_SOURCE_DIR}/src/fontset.c" "${PROJECT_SOURCE_DIR}/src/fontrules.c"
  "${PROJECT_SOURCE_DIR}/src/fileio.c" "${PROJECT_SOURCE_DIR}/src/alloc.c")
target_include_directories(test_fontset PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(test_fontset PRIVATE FONTSET_TEST_FONTS="${PROJECT_SOURCE_DIR}/assets/fonts"
  FONTSET_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fonts")
target_link_libraries(test_fontset SDL3_ttf::SDL3_ttf SDL3::SDL3)
link_fileio(test_fontset)
add_test(NAME fontset COMMAND test_fontset)

# The fallback candidates: fontconfig's sort over a fixture file on Linux (its one folder is the
# bundled fonts, its cache the test's own), the runner's Fonts registry key on Windows. The test stands
# in for find_default_font() and the launcher's log
add_executable(test_fontcand test_fontcand.c "${PROJECT_SOURCE_DIR}/src/fontcand.c" "${PROJECT_SOURCE_DIR}/src/fontrules.c")
target_include_directories(test_fontcand PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(test_fontcand PRIVATE FONTCAND_TEST_FONTS="${PROJECT_SOURCE_DIR}/assets/fonts")
target_link_libraries(test_fontcand SDL3_ttf::SDL3_ttf SDL3::SDL3)
add_test(NAME fontcand COMMAND test_fontcand)
if (UNIX)
  target_link_libraries(test_fontcand Fontconfig::Fontconfig)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fontcand-fonts.conf"
    "<?xml version=\"1.0\"?>\n<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n<fontconfig>\n"
    "  <dir>${PROJECT_SOURCE_DIR}/assets/fonts</dir>\n"
    "  <cachedir>${CMAKE_CURRENT_BINARY_DIR}/fontcand-cache</cachedir>\n</fontconfig>\n")
  set_tests_properties(fontcand PROPERTIES ENVIRONMENT "FONTCONFIG_FILE=${CMAKE_CURRENT_BINARY_DIR}/fontcand-fonts.conf")
else ()
  target_link_libraries(test_fontcand advapi32)
endif ()
