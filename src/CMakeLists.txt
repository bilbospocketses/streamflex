#Build main launcher executable file
set(SOURCES
  launcher.c
  launcher.h
  util.c
  util.h
  image.c
  image.h
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
  config_fields.c
  config_fields.h
  listpick.c
  listpick.h
  colourpick.c
  colourpick.h
  fontlist.c
  fontlist.h
  fontscan.c
  fontscan.h
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
target_link_libraries(${EXECUTABLE_TITLE} platform)
if (UNIX)
  target_link_libraries(${EXECUTABLE_TITLE} PkgConfig::SDL2 PkgConfig::SDL2_IMAGE PkgConfig::SDL2_TTF PkgConfig::INIH m)
else ()
  target_link_libraries(${EXECUTABLE_TITLE} 
  $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
	$<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
	$<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
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
