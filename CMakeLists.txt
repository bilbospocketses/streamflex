# Project info
cmake_minimum_required(VERSION 3.18)
project("StreamFlex" 
  LANGUAGES C
  VERSION 0.4.1
  DESCRIPTION "Customizable HTPC Application Launcher"
  HOMEPAGE_URL "https://github.com/bilbospocketses/streamflex"
)
set(EXECUTABLE_TITLE "streamflex")
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")
include_directories(${PROJECT_BINARY_DIR})

option(EXTRA_WARNINGS "Enable extra compiler warnings" OFF)
if (EXTRA_WARNINGS)
  if (MSVC)
    add_compile_options(/W4 /WX)
  else ()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
  endif ()
endif ()

# MSVC calls the POSIX names (strdup) deprecated in favor of its own (_strdup); the code uses the
# POSIX names on every platform. This reaches the launcher, platform and test targets, not vcpkg's builds
if (MSVC)
  add_compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)
endif ()

# Set Visual Studio solution startup project
if (WIN32)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${EXECUTABLE_TITLE})
endif ()

# Minimum library versions for Linux
set(MIN_SDL_VERSION "2.0.18")
set(MIN_SDL_IMAGE_VERSION "2.0.5")
set(MIN_SDL_TTF_VERSION "2.0.15")

# Generate PKGBUILD script for Arch packages
if (PACKAGE STREQUAL "ARCH")
  configure_file("${PROJECT_SOURCE_DIR}/config/PKGBUILD.in" "${PROJECT_BINARY_DIR}/PKGBUILD")
  return()
endif ()
        
# Import settings
include(${PROJECT_SOURCE_DIR}/config/config_settings.cmake)

# Find dependencies - Linux
if (UNIX)
  find_package(PkgConfig MODULE REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2>=${MIN_SDL_VERSION})
  pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image>=${MIN_SDL_IMAGE_VERSION})
  pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf>=${MIN_SDL_TTF_VERSION})
  pkg_check_modules(INIH REQUIRED IMPORTED_TARGET inih)
endif ()

# Find dependencies - Windows
if (WIN32)
  find_package(SDL2 CONFIG REQUIRED)
  find_package(sdl2_image CONFIG REQUIRED)
  find_package(SDL2_ttf CONFIG REQUIRED)
  
  # A function to find a vcpkg library once per configuration, so each configuration links its own.
  # A multi-config generator has no build type, so vcpkg searches its debug folder first, and a
  # plain find_library() gave the Release link the debug library and its debug C runtime (LIBCMTD)
  include(SelectLibraryConfigurations)
  function(find_vcpkg_library var name)
    set(VCPKG_TRIPLET_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    find_library(${var}_LIBRARY_RELEASE ${name} PATHS "${VCPKG_TRIPLET_DIR}/lib" NO_DEFAULT_PATH REQUIRED)
    # Not REQUIRED: a release-only triplet (VCPKG_BUILD_TYPE release) has no debug folder, and
    # select_library_configurations() then links the release library in every configuration
    find_library(${var}_LIBRARY_DEBUG ${name} PATHS "${VCPKG_TRIPLET_DIR}/debug/lib" NO_DEFAULT_PATH)
    select_library_configurations(${var})
    # Drop the single path an older configure cached under the same name
    unset(${var} CACHE)
    set(${var} ${${var}_LIBRARY} PARENT_SCOPE)
  endfunction()

  find_path(INIH_INCLUDE_DIR "ini.h" REQUIRED)
  find_vcpkg_library(INIH inih)
  find_path(GETOPT_INCLUDE_DIR "getopt.h" REQUIRED)
  find_vcpkg_library(GETOPT getopt)

  # A static triplet's libraries are built against the static C runtime (/MT), so build ours the
  # same way; the default /MD made the link search both MSVCRT and LIBCMT. A runtime given on the
  # command line (-DCMAKE_MSVC_RUNTIME_LIBRARY=...) still wins
  if (VCPKG_TARGET_TRIPLET MATCHES "-static$" AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif ()
endif ()

# Build configuration - Linux
if (UNIX)
  if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -s")
  endif ()
  set(CMAKE_SHARED_LINKER_FLAGS "--as-needed")
endif ()

 # Configure default applications - Linux
 if (UNIX)
  set(DESKTOP_PATH "/usr/share/applications")
  set(CMD_KODI "${DESKTOP_PATH}/kodi.desktop")
  set(CMD_PLEX "${DESKTOP_PATH}/plexmediaplayer.desktop;TVF")
  if (RPI)
    set(TITLE_GAMES "RetroArch")
    set(ICON_GAMES "retroarch")
    set(CMD_GAMES "${DESKTOP_PATH}/retroarch.desktop")
  else ()
    set(TITLE_GAMES "Steam")
    set(ICON_GAMES "steam")
    set(CMD_GAMES "${DESKTOP_PATH}/steam.desktop;BigPicture")
  endif ()
endif ()

# Configure default applications - Windows
if (WIN32)
  set(CMD_KODI "\"C:\\Program Files\\Kodi\\kodi.exe\"")
  set(CMD_PLEX "\"C:\\Program Files\\Plex\\Plex Media Player\\PlexMediaPlayer.exe\"")
  set(TITLE_GAMES "Steam")
  set(ICON_GAMES "steam")
  set(CMD_GAMES "\"C:\\Program Files (x86)\\Steam\\steam.exe\" steam://open/bigpicture")
endif ()

# Generate Windows application manifest
if (WIN32)
  set(VERSION_M ${PROJECT_VERSION_MAJOR})
  if (PROJECT_VERSION_MINOR)
    set(VERSION_N ${PROJECT_VERSION_MINOR})
  else ()
    set(VERSION_N 0)
  endif()
  if (PROJECT_VERSION_PATCH)
    set(VERSION_O ${PROJECT_VERSION_PATCH})
  else ()
    set(VERSION_O 0)
  endif()
  if (PROJECT_VERSION_TWEAK)
    set(VERSION_P ${PROJECT_VERSION_TWEAK})
  else ()
    set(VERSION_P 0)
  endif()
  configure_file(
    ${PROJECT_SOURCE_DIR}/config/${EXECUTABLE_TITLE}.manifest.in 
	${PROJECT_BINARY_DIR}/${EXECUTABLE_TITLE}.manifest
  )
endif()

# Set time format strings
if (WIN32)
  set(REMOVE_ZERO "#")
else()
  set(REMOVE_ZERO "-")
endif ()
set(TIME_STRING_12HR "%${REMOVE_ZERO}I:%M %p")
set(TIME_STRING_24HR "%H:%M")
set(DATE_STRING_LITTLE "%d %b")
set(DATE_STRING_BIG "%b %${REMOVE_ZERO}d")

# Configure main header file  
configure_file("${PROJECT_SOURCE_DIR}/config/launcher_config.h.in" "${PROJECT_BINARY_DIR}/launcher_config.h")

#Build source files
add_subdirectory("src")

# Unit tests, run with ctest
enable_testing()
add_subdirectory("tests")

# Installation - Linux
if (UNIX)
  set(INSTALL_DIR_BIN "${CMAKE_INSTALL_PREFIX}/bin")
  set(INSTALL_DIR_SHARE "${CMAKE_INSTALL_PREFIX}/share/${EXECUTABLE_TITLE}")
  set(INSTALL_DIR_DESKTOP "${CMAKE_INSTALL_PREFIX}/share/applications")
  set(INSTALL_DIR_CONFIGFILE "${INSTALL_DIR_SHARE}")
  set(INSTALL_DIR_ASSETS "${INSTALL_DIR_SHARE}")
  install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/licenses/${EXECUTABLE_TITLE}")

  # Configure Debian packages
  if (PACKAGE STREQUAL "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME ${EXECUTABLE_TITLE})
    set(CPACK_DEBIAN_PACKAGE_VERSION ${CMAKE_PROJECT_VERSION})
    # No epoch. v0.1.0 shipped as flex-launcher 1:0.1.0 to sort above the
    # original project's 2.2; streamflex is a new package name with nothing
    # to outrank. Never add one later: an epoch cannot be removed again.
    if (NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
      set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    endif ()
    # Explicit name <package>_<version>_<arch>.deb, which the Release job and
    # the docs download page match on.
    set(CPACK_DEBIAN_FILE_NAME "${CPACK_DEBIAN_PACKAGE_NAME}_${CMAKE_PROJECT_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")
    # dpkg-shlibdeps derives every dependency and its minimum version (glibc
    # included) from the built binary. No explicit CPACK_DEBIAN_PACKAGE_DEPENDS:
    # CPack appends shlibdeps' list to it without merging, so each library
    # would appear twice. SDL2's floor comes from its symbols file:
    # SDL_RenderSetVSync (2.0.18) makes the package depend on libsdl2-2.0-0
    # (>= 2.0.18), matching MIN_SDL_VERSION above.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "bilbospocketses <104535858+bilbospocketses@users.noreply.github.com>")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${CMAKE_PROJECT_HOMEPAGE_URL})
    set(CPACK_DEBIAN_PACKAGE_SECTION "video")
    set(CPACK_DEBIAN_ARCHIVE_TYPE "gnutar")
    set(CPACK_DEBIAN_COMPRESSION_TYPE "gzip")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
  endif ()
endif ()

# Installation - Windows
if (WIN32)
  set(INSTALL_DIR_BIN ".")
  set(INSTALL_DIR_CONFIGFILE ".")
  set(INSTALL_DIR_ASSETS ".")

  configure_file(${PROJECT_SOURCE_DIR}/CHANGELOG.md ${PROJECT_BINARY_DIR}/CHANGELOG.txt COPYONLY)
  install(FILES ${PROJECT_BINARY_DIR}/CHANGELOG.txt DESTINATION "${INSTALL_DIR_BIN}")
  configure_file(${PROJECT_SOURCE_DIR}/LICENSE ${PROJECT_BINARY_DIR}/LICENSE.txt COPYONLY)
  install(FILES ${PROJECT_BINARY_DIR}/LICENSE.txt DESTINATION "${INSTALL_DIR_BIN}")
  
  # Copy the VC Runtime DLL in case user doesn't have it installed. An exe built with the static
  # C runtime (/MT, a -static triplet) imports no VC Runtime DLL, so only a /MD build ships one
  if (NOT CMAKE_MSVC_RUNTIME_LIBRARY OR CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    foreach(lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
      string(FIND ${lib} "vcruntime140.dll" vcruntime)
      if (NOT vcruntime EQUAL -1)
        install(FILES ${lib} DESTINATION ".")
      endif ()
    endforeach()
  endif ()
  
  # Set package
  if (NOT PACKAGE)
    set(PACKAGE "ZIP")
  endif ()
endif ()

# Installation - Common
install(TARGETS ${EXECUTABLE_TITLE} DESTINATION "${INSTALL_DIR_BIN}")
install(FILES "${PROJECT_BINARY_DIR}/install/config.ini" DESTINATION "${INSTALL_DIR_CONFIGFILE}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets" DESTINATION "${INSTALL_DIR_ASSETS}")
if (PACKAGE)
  set(CPACK_PACKAGE_NAME ${EXECUTABLE_TITLE})
  set(CPACK_GENERATOR ${PACKAGE})
  include(CPack)
endif ()

#Configure Linux application icons and .desktop file
if (UNIX)
  configure_file("${PROJECT_SOURCE_DIR}/config/launcher.desktop.in" "${PROJECT_BINARY_DIR}/install/${EXECUTABLE_TITLE}.desktop")
  install(FILES "${PROJECT_BINARY_DIR}/install/${EXECUTABLE_TITLE}.desktop" DESTINATION "${INSTALL_DIR_DESKTOP}")
  set(INSTALL_DIR_ICONS "${CMAKE_INSTALL_PREFIX}/share/icons")
  install(FILES "${PROJECT_SOURCE_DIR}/docs/streamflex.png" DESTINATION "${INSTALL_DIR_ICONS}/hicolor/48x48/apps")
  install(FILES "${PROJECT_SOURCE_DIR}/docs/streamflex.svg" DESTINATION "${INSTALL_DIR_ICONS}/hicolor/scalable/apps")
  # Small sizes are drawn from the simplified icon master (branding/icon); desktops
  # prefer an exact-size PNG over scaling the detailed 48 px or scalable artwork down.
  foreach (ICON_SIZE 16 24 32)
    install(FILES "${PROJECT_SOURCE_DIR}/docs/streamflex-${ICON_SIZE}.png"
      DESTINATION "${INSTALL_DIR_ICONS}/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps"
      RENAME "${EXECUTABLE_TITLE}.png")
  endforeach ()
endif ()
  
# Set paths for config files
set(FONT_PREFIX "${PROJECT_BINARY_DIR}/assets/fonts/")
file(COPY "${PROJECT_SOURCE_DIR}/assets" DESTINATION ${PROJECT_BINARY_DIR})

# Generate build config file
if (WIN32)
  string(REPLACE "/" "\\" FONT_PREFIX ${FONT_PREFIX})
endif ()
configure_file("${PROJECT_SOURCE_DIR}/config/config.ini.in" "${PROJECT_BINARY_DIR}/config.ini")

# Generate install config file
if (WIN32)
  set(PATH_ASSETS_RELATIVE "./assets")
  set(FONT_PREFIX "${PATH_ASSETS_RELATIVE}/fonts/")
else ()
  set(FONT_PREFIX "${INSTALL_DIR_ASSETS}/assets/fonts/")
endif ()

if (WIN32)
  string(REPLACE "/" "\\" FONT_PREFIX ${FONT_PREFIX})
endif ()
configure_file("${PROJECT_SOURCE_DIR}/config/config.ini.in" "${PROJECT_BINARY_DIR}/install/config.ini")
