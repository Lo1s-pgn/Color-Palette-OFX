#pragma once

#include <cstdint>
#include <string>

namespace LSPPaletteRuntimeEnv {

bool gpuStageDebugEnabled();
bool forceStageCopyEnabled();
bool openclDisableEnabled();
bool openclForceEnabled();
bool preferHostMetal();
bool preferHostCuda();

void logGpuBackend(const char* backend);
void logCacheStats(uint64_t hits, uint64_t misses);
void logStage(const char* message);
void logStageLine(const std::string& message);

} // namespace LSPPaletteRuntimeEnv
