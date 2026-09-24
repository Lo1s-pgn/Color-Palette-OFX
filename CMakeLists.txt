# LSP - Color Palette OFX — unified CMake (macOS + Windows + Linux, CPU render path)
cmake_minimum_required(VERSION 3.22)

option(PALETTE_OFX_FAT_ARCHS "Link universal Mach-O (arm64 + x86_64)" ON)

if(CMAKE_HOST_APPLE)
  set(PALETTE_MACOSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "${PALETTE_MACOSX_DEPLOYMENT_TARGET}" CACHE STRING "" FORCE)
  if(PALETTE_OFX_FAT_ARCHS)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS build architectures" FORCE)
  endif()
elseif(NOT CMAKE_HOST_WIN32 AND NOT CMAKE_HOST_UNIX)
  message(FATAL_ERROR "LSP Color Palette CMake supports macOS, Windows, and Linux only.")
endif()

if(CMAKE_HOST_APPLE)
  project(LSP_Color_Palette
    VERSION 0.0.0
    DESCRIPTION "LSP - Color Palette OFX"
    LANGUAGES C CXX OBJCXX
  )
elseif(CMAKE_HOST_WIN32)
  project(LSP_Color_Palette
    VERSION 0.0.0
    DESCRIPTION "LSP - Color Palette OFX"
    LANGUAGES C CXX
  )
else()
  project(LSP_Color_Palette
    VERSION 0.0.0
    DESCRIPTION "LSP - Color Palette OFX"
    LANGUAGES C CXX
  )
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ColorPaletteVersion.cmake")

if(APPLE)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ColorPaletteApple.cmake")
elseif(WIN32)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ColorPaletteWindows.cmake")
elseif(UNIX)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ColorPaletteLinux.cmake")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/scripts/archive_version.sh")
  add_custom_target(colorpalette_archive
    COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/archive_version.sh"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
    COMMENT "Archive current version, bump VERSION, rebuild"
  )
endif()
