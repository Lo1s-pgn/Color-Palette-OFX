set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
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

set(PALETTE_PLATFORM_EXTRA_SRCS "${CMAKE_SOURCE_DIR}/plugin/metal/LSPPaletteGridBlurMPS.mm")
set(PALETTE_PLATFORM_INCLUDE_DIRS "")
set(PALETTE_EXTRA_TARGET_DEPS colorpalette_gen_plist)

set(PALETTE_COMPILE_OPTIONS
  -O2
  -Wno-dynamic-exception-spec
  -fvisibility=hidden
)
set(PALETTE_COMPILE_DEFINITIONS "")
find_library(PALETTE_FRAMEWORK_FOUNDATION NAMES Foundation REQUIRED)
find_library(PALETTE_FRAMEWORK_METAL NAMES Metal REQUIRED)
find_library(PALETTE_FRAMEWORK_MPS NAMES MetalPerformanceShaders REQUIRED)
set(PALETTE_LINK_LIBS
  ${PALETTE_FRAMEWORK_FOUNDATION}
  ${PALETTE_FRAMEWORK_METAL}
  ${PALETTE_FRAMEWORK_MPS}
)
set(PALETTE_LINK_OPTIONS
  "-bundle"
  "-fvisibility=hidden"
  "-Wl,-rpath,@loader_path"
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

message(STATUS "[Palette] macOS bundle: ${PALETTE_OFX_BUNDLE}")
if(PALETTE_OFX_FAT_ARCHS)
  message(STATUS "[Palette] macOS architectures: ${CMAKE_OSX_ARCHITECTURES}")
else()
  message(STATUS "[Palette] macOS architectures: host default")
endif()
