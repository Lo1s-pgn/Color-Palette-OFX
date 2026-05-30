if(NOT DEFINED INPUT_FILE OR NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "GenerateLSPPaletteCLSource.cmake requires INPUT_FILE and OUTPUT_FILE")
endif()

file(READ "${INPUT_FILE}" OPENCL_SOURCE_CONTENT)
string(REPLACE "\\" "\\\\" OPENCL_SOURCE_ESCAPED "${OPENCL_SOURCE_CONTENT}")
string(REPLACE "\"" "\\\"" OPENCL_SOURCE_ESCAPED "${OPENCL_SOURCE_ESCAPED}")
string(REPLACE "\r\n" "\n" OPENCL_SOURCE_ESCAPED "${OPENCL_SOURCE_ESCAPED}")
string(REPLACE "\r" "\n" OPENCL_SOURCE_ESCAPED "${OPENCL_SOURCE_ESCAPED}")
string(REPLACE "\n" "\\n\"\n\"" OPENCL_SOURCE_ESCAPED "${OPENCL_SOURCE_ESCAPED}")

set(HEADER_TEXT
"#pragma once

#include <cstddef>

static constexpr const char kLSPPaletteCLSource[] =
\"${OPENCL_SOURCE_ESCAPED}\";

static constexpr std::size_t kLSPPaletteCLSourceSize = sizeof(kLSPPaletteCLSource) - 1;
")

file(WRITE "${OUTPUT_FILE}" "${HEADER_TEXT}")
