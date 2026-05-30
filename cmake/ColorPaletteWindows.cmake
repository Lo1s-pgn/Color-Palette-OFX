set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PALETTE_BUILD_ROOT "${CMAKE_SOURCE_DIR}/build/windows" CACHE PATH "Windows build intermediates")
set(PALETTE_DIST_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/release/${PALETTE_RELEASE_FOLDER_WINDOWS}")
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

enable_language(CUDA)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
  set(CMAKE_CUDA_ARCHITECTURES 75 86 89)
endif()
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -allow-unsupported-compiler")
find_package(CUDAToolkit REQUIRED)

set(_palette_opencl_lib "")
foreach(_dir
    "${CUDAToolkit_LIBRARY_DIR}"
    "${CUDAToolkit_LIBRARY_ROOT}/lib/x64"
    "${CUDAToolkit_ROOT_DIR}/lib/x64"
  )
  if(EXISTS "${_dir}/OpenCL.lib")
    set(_palette_opencl_lib "${_dir}/OpenCL.lib")
    break()
  endif()
endforeach()
if(_palette_opencl_lib STREQUAL "" AND DEFINED ENV{CUDA_PATH})
  if(EXISTS "$ENV{CUDA_PATH}/lib/x64/OpenCL.lib")
    set(_palette_opencl_lib "$ENV{CUDA_PATH}/lib/x64/OpenCL.lib")
  endif()
endif()
if(_palette_opencl_lib STREQUAL "")
  message(FATAL_ERROR "OpenCL.lib not found (CUDA Toolkit opencl component required on Windows).")
endif()

set(OPENCL_KERNEL_SRC "${CMAKE_SOURCE_DIR}/plugin/opencl/LSPPalette.cl")
set(OPENCL_KERNEL_GEN_SCRIPT "${CMAKE_SOURCE_DIR}/plugin/opencl/GenerateLSPPaletteCLSource.cmake")
set(OPENCL_KERNEL_HDR_DIR "${PALETTE_BUILD_ROOT}/generated")
set(OPENCL_KERNEL_HDR "${OPENCL_KERNEL_HDR_DIR}/LSPPaletteCLSource.h")
file(MAKE_DIRECTORY "${OPENCL_KERNEL_HDR_DIR}")
add_custom_command(
  OUTPUT "${OPENCL_KERNEL_HDR}"
  COMMAND ${CMAKE_COMMAND}
    -DINPUT_FILE=${OPENCL_KERNEL_SRC}
    -DOUTPUT_FILE=${OPENCL_KERNEL_HDR}
    -P "${OPENCL_KERNEL_GEN_SCRIPT}"
  DEPENDS "${OPENCL_KERNEL_SRC}" "${OPENCL_KERNEL_GEN_SCRIPT}"
  VERBATIM
)
add_custom_target(LSPPaletteOpenCLSourceHeader ALL DEPENDS "${OPENCL_KERNEL_HDR}")

set(PALETTE_PLATFORM_EXTRA_SRCS "${CMAKE_SOURCE_DIR}/plugin/cuda/LSPPalette.cu")
set(PALETTE_PLATFORM_INCLUDE_DIRS "${OPENCL_KERNEL_HDR_DIR}")
set(PALETTE_EXTRA_TARGET_DEPS colorpalette_gen_plist LSPPaletteOpenCLSourceHeader)

set(PALETTE_COMPILE_OPTIONS $<$<COMPILE_LANGUAGE:CXX>:/W3 /bigobj /MP /EHsc>)
set(PALETTE_COMPILE_DEFINITIONS
  OFX_SUPPORTS_CUDARENDER
  LSP_PALETTE_HAS_CUDA
  LSP_PALETTE_HAS_OPENCL
  WINDOWS
  _WINDOWS
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  _CRT_SECURE_NO_WARNINGS
)
set(PALETTE_LINK_LIBS CUDA::cudart_static "${_palette_opencl_lib}" shell32)
set(PALETTE_LINK_OPTIONS "")

include("${CMAKE_CURRENT_LIST_DIR}/ColorPaletteCommon.cmake")

target_include_directories(colorpalette_ofx PRIVATE ${CUDAToolkit_INCLUDE_DIRS})

colorpalette_assemble_bundle_postbuild("Win64")

message(STATUS "[Palette] Windows bundle: ${PALETTE_OFX_BUNDLE}")
