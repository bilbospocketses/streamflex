# Build platform-specific library
if (UNIX)
  add_library(platform unix.c unix.h platform.h)
endif ()
if (WIN32)
  add_library(platform win32.c platform.h)
endif ()
target_link_libraries(platform SDL3::SDL3)
