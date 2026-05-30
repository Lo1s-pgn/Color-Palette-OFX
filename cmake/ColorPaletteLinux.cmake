set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PALETTE_LINUX_CUDA "Build Linux CUDA backend" ON)

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

find_package(OpenCL REQUIRED)

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

set(PALETTE_PLATFORM_EXTRA_SRCS "")
set(PALETTE_PLATFORM_INCLUDE_DIRS "${OPENCL_KERNEL_HDR_DIR}")
set(PALETTE_EXTRA_TARGET_DEPS colorpalette_gen_plist LSPPaletteOpenCLSourceHeader)
set(PALETTE_COMPILE_DEFINITIONS LSP_PALETTE_HAS_OPENCL)

if(PALETTE_LINUX_CUDA)
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD 17)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
  find_package(CUDAToolkit REQUIRED)
  list(APPEND PALETTE_PLATFORM_EXTRA_SRCS "${CMAKE_SOURCE_DIR}/plugin/cuda/LSPPalette.cu")
  list(APPEND PALETTE_COMPILE_DEFINITIONS OFX_SUPPORTS_CUDARENDER LSP_PALETTE_HAS_CUDA)
  set(PALETTE_LINK_LIBS CUDA::cudart OpenCL::OpenCL)
else()
  set(PALETTE_LINK_LIBS OpenCL::OpenCL)
endif()

set(PALETTE_COMPILE_OPTIONS -O2 -Wno-dynamic-exception-spec -fvisibility=hidden)
set(PALETTE_LINK_OPTIONS -static-libstdc++ -static-libgcc)

include("${CMAKE_CURRENT_LIST_DIR}/ColorPaletteCommon.cmake")

if(PALETTE_LINUX_CUDA)
  target_include_directories(colorpalette_ofx PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
endif()

colorpalette_assemble_bundle_postbuild("Linux-x86-64")

message(STATUS "[Palette] Linux bundle: ${PALETTE_OFX_BUNDLE}")
