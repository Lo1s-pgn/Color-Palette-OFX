# Shared colorpalette_ofx target (included from ColorPaletteApple/Windows/Linux.cmake).

set(OFX_SDK_PATH "${CMAKE_SOURCE_DIR}/openfx-sdk" CACHE PATH "Path to OFX SDK (include + Support)")

if(NOT EXISTS "${OFX_SDK_PATH}/include/ofxCore.h")
  message(FATAL_ERROR "OFX_SDK_PATH must contain include/ofxCore.h (got '${OFX_SDK_PATH}')")
endif()

set(PALETTE_PLUGIN_CORE_SRCS
  "${CMAKE_SOURCE_DIR}/common/color/ColorManagement.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteUtil.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteImageAccess.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteAnalysis.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteExtract.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteComposite.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteDescribe.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteGpuParams.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteRender.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteRenderCache.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteRuntimeEnv.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteRenderProcessor.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPaletteProcessor.cpp"
  "${CMAKE_SOURCE_DIR}/plugin/core/LSPPalettePlugin.cpp"
)

set(PALETTE_OFX_SUPPORT_SRCS
  "${OFX_SDK_PATH}/Support/Library/ofxsCore.cpp"
  "${OFX_SDK_PATH}/Support/Library/ofxsImageEffect.cpp"
  "${OFX_SDK_PATH}/Support/Library/ofxsInteract.cpp"
  "${OFX_SDK_PATH}/Support/Library/ofxsLog.cpp"
  "${OFX_SDK_PATH}/Support/Library/ofxsMultiThread.cpp"
  "${OFX_SDK_PATH}/Support/Library/ofxsParams.cpp"
  "${OFX_SDK_PATH}/Support/Library/ofxsProperty.cpp"
  "${OFX_SDK_PATH}/Support/Library/ofxsPropertyValidation.cpp"
)

set(PALETTE_INCLUDE_DIRS
  "${CMAKE_SOURCE_DIR}/plugin"
  "${CMAKE_SOURCE_DIR}/plugin/core"
  "${CMAKE_SOURCE_DIR}/plugin/metal"
  "${CMAKE_SOURCE_DIR}/plugin/cuda"
  "${CMAKE_SOURCE_DIR}/plugin/opencl"
  "${CMAKE_SOURCE_DIR}/common/color"
  "${OFX_SDK_PATH}/include"
  "${OFX_SDK_PATH}/Support/include"
  "${OFX_SDK_PATH}/Support/Library"
)

file(MAKE_DIRECTORY "${PALETTE_BIN_DIR}")

add_library(colorpalette_ofx MODULE
  ${PALETTE_PLUGIN_CORE_SRCS}
  ${PALETTE_OFX_SUPPORT_SRCS}
  ${PALETTE_PLATFORM_EXTRA_SRCS}
)

target_include_directories(colorpalette_ofx PRIVATE ${PALETTE_INCLUDE_DIRS} ${PALETTE_PLATFORM_INCLUDE_DIRS})
target_compile_features(colorpalette_ofx PRIVATE cxx_std_20)
target_compile_options(colorpalette_ofx PRIVATE ${PALETTE_COMPILE_OPTIONS})
target_compile_definitions(colorpalette_ofx PRIVATE ${PALETTE_COMPILE_DEFINITIONS} OFX_SUPPORTS_OPENCLRENDER)
if(PALETTE_LINK_LIBS)
  target_link_libraries(colorpalette_ofx PRIVATE ${PALETTE_LINK_LIBS})
endif()
target_link_options(colorpalette_ofx PRIVATE ${PALETTE_LINK_OPTIONS})

set_target_properties(colorpalette_ofx PROPERTIES
  PREFIX ""
  SUFFIX ".ofx"
  OUTPUT_NAME "${PALETTE_OFX_BUNDLE_STEM}"
  LIBRARY_OUTPUT_DIRECTORY "${PALETTE_BIN_DIR}"
)

add_dependencies(colorpalette_ofx colorpalette_gen_version)
if(PALETTE_EXTRA_TARGET_DEPS)
  add_dependencies(colorpalette_ofx ${PALETTE_EXTRA_TARGET_DEPS})
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/ICON.png")
  set(PALETTE_HAS_ICON TRUE)
else()
  set(PALETTE_HAS_ICON FALSE)
endif()

function(colorpalette_assemble_bundle_postbuild p_PlatformSubdir)
  set(_bundle_ofx "${PALETTE_OFX_BUNDLE}/Contents/${p_PlatformSubdir}/${PALETTE_OFX_EXECUTABLE_NAME}")
  add_custom_command(TARGET colorpalette_ofx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PALETTE_DIST_PLATFORM_DIR}"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${PALETTE_OFX_BUNDLE}/Contents"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PALETTE_OFX_BUNDLE}/Contents/${p_PlatformSubdir}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PALETTE_OFX_BUNDLE}/Contents/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PALETTE_INFO_PLIST}" "${PALETTE_OFX_BUNDLE}/Contents/Info.plist"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:colorpalette_ofx>" "${_bundle_ofx}"
    COMMENT "Assemble ${PALETTE_OFX_BUNDLE_STEM}.ofx.bundle (${p_PlatformSubdir}) -> ${PALETTE_DIST_PLATFORM_DIR}"
  )
  if(PALETTE_HAS_ICON)
    add_custom_command(TARGET colorpalette_ofx POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CMAKE_SOURCE_DIR}/ICON.png"
              "${PALETTE_OFX_BUNDLE}/Contents/Resources/${PALETTE_PLUGIN_ICON_NAME}"
    )
  endif()
endfunction()

add_custom_target(colorpalette_all DEPENDS colorpalette_ofx)
