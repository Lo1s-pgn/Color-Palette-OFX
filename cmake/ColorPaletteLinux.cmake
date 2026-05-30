set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PALETTE_BUILD_ROOT "${CMAKE_SOURCE_DIR}/build/linux" CACHE PATH "Linux build intermediates")
set(PALETTE_DIST_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/release/${PALETTE_RELEASE_FOLDER_LINUX}")
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

set(PALETTE_PLATFORM_EXTRA_SRCS "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteGridBlurMPSStub.cpp")
set(PALETTE_PLATFORM_INCLUDE_DIRS "")
set(PALETTE_EXTRA_TARGET_DEPS colorpalette_gen_plist)

set(PALETTE_COMPILE_OPTIONS -O2 -Wno-dynamic-exception-spec -fvisibility=hidden)
set(PALETTE_COMPILE_DEFINITIONS "")
set(PALETTE_LINK_LIBS "")
set(PALETTE_LINK_OPTIONS -static-libstdc++ -static-libgcc)

include("${CMAKE_CURRENT_LIST_DIR}/ColorPaletteCommon.cmake")

colorpalette_assemble_bundle_postbuild("Linux-x86-64")

message(STATUS "[Palette] Linux bundle: ${PALETTE_OFX_BUNDLE}")
