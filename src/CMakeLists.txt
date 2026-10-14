#Build main launcher executable file
set(SOURCES
  launcher.c
  launcher.h
  util.c
  util.h
  image.c
  image.h
  iconload.c
  iconload.h
  debug.c
  debug.h
  clock.c
  clock.h
  layout.c
  layout.h
  utf8.c
  utf8.h
  chroma.c
  chroma.h
  videodriver.c
  videodriver.h
  padpower.c
  padpower.h
  library.c
  library.h
  alloc.c
  alloc.h
  fileio.c
  fileio.h
  inidoc.c
  inidoc.h
  config_save.c
  config_save.h
  settings.c
  settings.h
  derive.c
  derive.h
  winplan.c
  winplan.h
  vsyncwatch.c
  vsyncwatch.h
  config_fields.c
  config_fields.h
  listpick.c
  listpick.h
  colorpick.c
  colorpick.h
  fontlist.c
  fontlist.h
  fontscan.c
  fontscan.h
  fontrules.c
  fontrules.h
  fontset.c
  fontset.h
  fontcand.c
  bindings.c
  bindings.h
  keycodes.c
  keycodes.h
  browser.c
  browser.h
  settings_screen.c
  settings_screen.h
  settings_pickers.c
  settings_pickers.h
  settings_fonts.c
  settings_fonts.h
  test_hooks.c
  test_hooks.h
)
if (UNIX)
  add_executable(${EXECUTABLE_TITLE} ${SOURCES})
endif ()
if (WIN32)
  set(APP_ICON_RESOURCE_WINDOWS "${PROJECT_SOURCE_DIR}/config/${EXECUTABLE_TITLE}.rc")
  set(MANIFEST_FILE "${PROJECT_BINARY_DIR}/${EXECUTABLE_TITLE}.manifest")
  add_executable(${EXECUTABLE_TITLE} WIN32 ${SOURCES} ${MANIFEST_FILE} ${APP_ICON_RESOURCE_WINDOWS})
  set_property(TARGET ${EXECUTABLE_TITLE} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()

# The icon library and the default fonts are found next to the executable, so keep a copy of both beside it
# in the build tree as well: a build run from another folder then behaves like an installed copy
add_custom_command(TARGET ${EXECUTABLE_TITLE} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/assets/icons/library"
          "$<TARGET_FILE_DIR:${EXECUTABLE_TITLE}>/assets/icons/library"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/assets/fonts"
          "$<TARGET_FILE_DIR:${EXECUTABLE_TITLE}>/assets/fonts")

# Build libraries
add_subdirectory("platform")

# Link libraries together
target_link_libraries(${EXECUTABLE_TITLE} platform SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image SDL3::SDL3)
if (UNIX)
  target_link_libraries(${EXECUTABLE_TITLE} Fontconfig::Fontconfig PkgConfig::INIH m)
else ()
  target_link_libraries(${EXECUTABLE_TITLE}
    ${INIH}
    ${GETOPT}
    PowrProf
    shell32
    ole32
    uuid
    advapi32
  )
  target_include_directories(${EXECUTABLE_TITLE} PUBLIC ${INIH_INCLUDE_DIR} ${GETOPT_INCLUDE_DIR})
endif ()
target_include_directories(${EXECUTABLE_TITLE} SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external")
