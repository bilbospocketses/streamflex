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
# through bindings.c's model (whose lines the tests parse with inidoc.c), and names a colour through
# colourpick.c's swatches (whose contrast maths needs libm)
add_executable(test_settings test_settings.c "${PROJECT_SOURCE_DIR}/src/settings.c" "${PROJECT_SOURCE_DIR}/src/layout.c"
  "${PROJECT_SOURCE_DIR}/src/fileio.c" "${PROJECT_SOURCE_DIR}/src/alloc.c" "${PROJECT_SOURCE_DIR}/src/bindings.c"
  "${PROJECT_SOURCE_DIR}/src/inidoc.c" "${PROJECT_SOURCE_DIR}/src/colourpick.c")
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

# Unit tests for the colour picker's model and the contrast maths (pure; the maths needs libm)
add_executable(test_colourpick test_colourpick.c "${PROJECT_SOURCE_DIR}/src/colourpick.c")
target_include_directories(test_colourpick PRIVATE "${PROJECT_SOURCE_DIR}/src")
if (UNIX)
  target_link_libraries(test_colourpick m)
endif ()
add_test(NAME colourpick COMMAND test_colourpick)

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
# (colourpick.c allocates nothing; settings.c names colours through it)
add_executable(test_alloc test_alloc.c "${PROJECT_SOURCE_DIR}/src/alloc.c" "${PROJECT_SOURCE_DIR}/src/fileio.c"
  "${PROJECT_SOURCE_DIR}/src/inidoc.c" "${PROJECT_SOURCE_DIR}/src/config_save.c" "${PROJECT_SOURCE_DIR}/src/browser.c"
  "${PROJECT_SOURCE_DIR}/src/settings.c" "${PROJECT_SOURCE_DIR}/src/layout.c" "${PROJECT_SOURCE_DIR}/src/listpick.c"
  "${PROJECT_SOURCE_DIR}/src/fontlist.c" "${PROJECT_SOURCE_DIR}/src/bindings.c" "${PROJECT_SOURCE_DIR}/src/colourpick.c")
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
