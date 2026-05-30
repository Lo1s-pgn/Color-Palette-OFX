#include "LSPPaletteRuntimeEnv.h"

#include "LSPPaletteLog.h"

#include <cstdlib>
#include <string>

namespace {

bool envFlag(const char* name) {
    const char* v = std::getenv(name);
    if (v == nullptr || v[0] == '\0')
        return false;
    return !(v[0] == '0' && v[1] == '\0');
}

} // namespace

namespace LSPPaletteRuntimeEnv {

bool gpuStageDebugEnabled() {
    static const bool on = envFlag("LSP_PALETTE_GPU_STAGE_DEBUG");
    return on;
}

bool forceStageCopyEnabled() {
    static const bool on = envFlag("LSP_PALETTE_FORCE_STAGE_COPY");
    return on;
}

bool openclDisableEnabled() {
    static const bool on = envFlag("LSP_PALETTE_DISABLE_OPENCL");
    return on;
}

bool openclForceEnabled() {
    static const bool on = envFlag("LSP_PALETTE_FORCE_OPENCL");
    return on;
}

bool preferHostMetal() {
    const char* v = std::getenv("LSP_PALETTE_METAL_RENDER_MODE");
    if (v != nullptr && v[0] != '\0') {
        if (v[0] == 'I' || v[0] == 'i')
            return false;
        if (v[0] == 'H' || v[0] == 'h')
            return true;
    }
    return true;
}

bool preferHostCuda() {
    const char* v = std::getenv("LSP_PALETTE_RENDER_MODE");
    if (v != nullptr && (v[0] == 'H' || v[0] == 'h'))
        return true;
    return envFlag("LSP_PALETTE_ENABLE_OFX_HOST_CUDA");
}

void logGpuBackend(const char* backend) {
    if (!gpuStageDebugEnabled() || backend == nullptr)
        return;
    LSPPaletteLog::writeInfoLine(std::string("gpu_backend=") + backend);
}

void logCacheStats(uint64_t hits, uint64_t misses) {
    if (!gpuStageDebugEnabled())
        return;
    LSPPaletteLog::writeInfoLine("cache_hits=" + std::to_string(hits) + " misses=" + std::to_string(misses));
}

void logStage(const char* message) {
    if (!gpuStageDebugEnabled() || message == nullptr)
        return;
    LSPPaletteLog::writeInfoLine(message);
}

void logStageLine(const std::string& message) {
    if (!gpuStageDebugEnabled())
        return;
    LSPPaletteLog::writeInfoLine(message);
}

} // namespace LSPPaletteRuntimeEnv
