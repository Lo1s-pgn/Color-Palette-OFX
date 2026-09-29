set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

set(PALETTE_BUILD_ROOT "${CMAKE_SOURCE_DIR}/build/macos" CACHE PATH "macOS build intermediates")
set(PALETTE_DIST_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/release/${PALETTE_RELEASE_FOLDER_MACOS}")
set(PALETTE_BIN_DIR "${PALETTE_BUILD_ROOT}/bin")
set(PALETTE_INFO_PLIST "${PALETTE_BUILD_ROOT}/Info.plist")
set(PALETTE_OFX_BUNDLE "${PALETTE_DIST_PLATFORM_DIR}/${PALETTE_OFX_BUNDLE_STEM}.ofx.bundle")

file(MAKE_DIRECTORY "${PALETTE_BUILD_ROOT}")
configure_file(
  "${CMAKE_SOURCE_DIR}/Info.plist.in"
  "${PALETTE_INFO_PLIST}"
  @ONLY
)
add_custom_target(colorpalette_gen_plist DEPENDS "${PALETTE_INFO_PLIST}")

set(PALETTE_PLATFORM_EXTRA_SRCS
  "${CMAKE_SOURCE_DIR}/plugin/metal/LSPPaletteMetal.mm"
  "${CMAKE_SOURCE_DIR}/plugin/metal/LSPPaletteExtractGpu.mm"
)
set(PALETTE_PLATFORM_INCLUDE_DIRS "")
set(PALETTE_EXTRA_TARGET_DEPS colorpalette_gen_plist)

set(PALETTE_COMPILE_OPTIONS
  -O2
  -Wno-dynamic-exception-spec
  -fvisibility=hidden
)
set(PALETTE_COMPILE_DEFINITIONS "")
set(PALETTE_LINK_LIBS "")
set(PALETTE_LINK_OPTIONS
  "-bundle"
  "-fvisibility=hidden"
  "-Wl,-rpath,@loader_path"
)

find_program(XCRUN_EXECUTABLE xcrun REQUIRED)
set(METAL_SRC_COMPOSITE "${CMAKE_SOURCE_DIR}/plugin/metal/LSPPalette.metal")
set(METAL_SRC_EXTRACT "${CMAKE_SOURCE_DIR}/plugin/metal/LSPPaletteExtract.metal")
set(METAL_AIR_COMPOSITE "${PALETTE_BUILD_ROOT}/LSPPalette.air")
set(METAL_AIR_EXTRACT "${PALETTE_BUILD_ROOT}/LSPPaletteExtract.air")
set(METAL_LIB "${PALETTE_BUILD_ROOT}/LSPPalette.metallib")
if(CMAKE_OSX_DEPLOYMENT_TARGET)
  set(METAL_MIN_OS "${CMAKE_OSX_DEPLOYMENT_TARGET}")
else()
  set(METAL_MIN_OS "11.0")
endif()

add_custom_command(
  OUTPUT "${METAL_AIR_COMPOSITE}"
  COMMAND "${XCRUN_EXECUTABLE}" -sdk macosx metal -std=macos-metal2.4 -mmacosx-version-min=${METAL_MIN_OS} -c "${METAL_SRC_COMPOSITE}" -o "${METAL_AIR_COMPOSITE}"
  DEPENDS "${METAL_SRC_COMPOSITE}" "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteGpuParamsShared.h" "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteGpuDownsampleParams.h"
  VERBATIM
)

add_custom_command(
  OUTPUT "${METAL_AIR_EXTRACT}"
  COMMAND "${XCRUN_EXECUTABLE}" -sdk macosx metal -std=macos-metal2.4 -mmacosx-version-min=${METAL_MIN_OS} -c "${METAL_SRC_EXTRACT}" -o "${METAL_AIR_EXTRACT}"
  DEPENDS "${METAL_SRC_EXTRACT}" "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteGpuExtractShared.h"
  VERBATIM
)

add_custom_command(
  OUTPUT "${METAL_LIB}"
  COMMAND "${XCRUN_EXECUTABLE}" -sdk macosx metallib "${METAL_AIR_COMPOSITE}" "${METAL_AIR_EXTRACT}" -o "${METAL_LIB}"
  DEPENDS "${METAL_AIR_COMPOSITE}" "${METAL_AIR_EXTRACT}"
  VERBATIM
)

add_custom_target(LSPPaletteMetalLib ALL DEPENDS "${METAL_LIB}")
list(APPEND PALETTE_EXTRA_TARGET_DEPS LSPPaletteMetalLib)

find_library(PALETTE_FRAMEWORK_METAL NAMES Metal REQUIRED)
find_library(PALETTE_FRAMEWORK_FOUNDATION NAMES Foundation REQUIRED)
set(PALETTE_LINK_LIBS
  ${PALETTE_FRAMEWORK_METAL}
  ${PALETTE_FRAMEWORK_FOUNDATION}
)

include("${CMAKE_CURRENT_LIST_DIR}/ColorPaletteCommon.cmake")

target_compile_options(colorpalette_ofx PRIVATE
  "$<$<COMPILE_LANGUAGE:OBJCXX>:-fblocks>"
)

add_custom_command(TARGET colorpalette_ofx POST_BUILD
  COMMAND strip -x "$<TARGET_FILE:colorpalette_ofx>"
  COMMENT "strip -x colorpalette_ofx"
)

colorpalette_assemble_bundle_postbuild("MacOS")

add_custom_command(TARGET colorpalette_ofx POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PALETTE_OFX_BUNDLE}/Contents/Resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${METAL_LIB}" "${PALETTE_OFX_BUNDLE}/Contents/Resources/LSPPalette.metallib"
  COMMENT "Copy LSPPalette.metallib into bundle Resources"
)

message(STATUS "[Palette] macOS bundle: ${PALETTE_OFX_BUNDLE}")
if(PALETTE_OFX_FAT_ARCHS)
  message(STATUS "[Palette] macOS architectures: ${CMAKE_OSX_ARCHITECTURES}")
else()
  message(STATUS "[Palette] macOS architectures: host default")
endif()
