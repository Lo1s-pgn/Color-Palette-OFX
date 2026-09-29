#include "LSPPaletteUtil.h"
#include "LSPPaletteLog.h"

#include <fstream>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__) || defined(__unix__)
#include <spawn.h>
#include <unistd.h>
extern "C" char** environ;
#endif

namespace {

#if defined(__APPLE__) || defined(__unix__)
bool spawnDetached(const char* program, const char* arg) {
    pid_t pid = 0;
    const char* argv[] = {program, arg, nullptr};
    if (posix_spawnp(&pid, program, nullptr, nullptr, const_cast<char* const*>(argv), environ) != 0)
        return false;
    return true;
}
#endif

void openPath(const std::string& path, const char* failTag) {
    if (path.empty())
        return;
#if defined(_WIN32)
    const HINSTANCE rc = ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(rc) <= 32)
        LSP_PALETTE_LOG_ERROR(failTag);
#elif defined(__APPLE__)
    if (!spawnDetached("open", path.c_str()))
        LSP_PALETTE_LOG_ERROR(failTag);
#else
    if (!spawnDetached("xdg-open", path.c_str()))
        LSP_PALETTE_LOG_ERROR(failTag);
#endif
}

} // namespace

void lspPaletteOpenUrl(const std::string& url) {
    openPath(url, "open_url_failed");
}

void lspPaletteOpenLogExternally() {
    const std::string path = LSPPaletteLog::getLogPath();
    LSPPaletteLog::ensureLogDirectoryExists(path);
    {
        std::ofstream touch(path, std::ios::app);
        (void)touch;
    }
    openPath(path, "open_log_failed");
}
