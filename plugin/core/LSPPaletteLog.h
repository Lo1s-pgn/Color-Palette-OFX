#pragma once
/* log file per OS, header written when plugin loads */
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#if defined(__APPLE__)
#include <dlfcn.h>
#include <limits.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace LSPPaletteLog {

inline std::mutex& getLogMutex() {
    static std::mutex s_mutex;
    return s_mutex;
}

inline const char* getHomeEnv() {
#if defined(_WIN32)
    const char* home = std::getenv("USERPROFILE");
    if (!home || home[0] == '\0')
        home = std::getenv("HOME");
#else
    const char* home = std::getenv("HOME");
#endif
    return home;
}

inline std::string getLogPath() {
#if defined(_WIN32)
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData && localAppData[0] != '\0')
        return (std::filesystem::path(localAppData) / "LSP" / "ColorPalette.log").string();
    return std::string("ColorPalette.log");
#elif defined(__APPLE__)
    const char* home = getHomeEnv();
    if (!home || home[0] == '\0')
        return std::string("/tmp/Palette.log");
    return std::string(home) + "/Library/Application Support/LSP/ColorPalette.log";
#else
    const char* home = getHomeEnv();
    if (!home || home[0] == '\0')
        return std::string("/tmp/Palette.log");
    return (std::filesystem::path(home) / ".cache" / "LSP" / "ColorPalette.log").string();
#endif
}

inline std::string sanitizePathForLog(const std::string& p) {
    const char* home = getHomeEnv();
    if (!home || home[0] == '\0' || p.empty())
        return p;
    std::string hp(home);
#if defined(_WIN32)
    if (p.size() >= hp.size() && _stricmp(p.substr(0, hp.size()).c_str(), hp.c_str()) == 0
        && (p.size() == hp.size() || p[hp.size()] == '\\' || p[hp.size()] == '/'))
        return std::string("~") + p.substr(hp.size());
#else
    if (p.size() >= hp.size() && p.compare(0, hp.size(), hp) == 0
        && (p.size() == hp.size() || p[hp.size()] == '/'))
        return std::string("~") + p.substr(hp.size());
#endif
    return p;
}

inline bool ensureLogDirectoryExists(const std::string& logPath) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path parent = fs::path(logPath).parent_path();
    if (parent.empty())
        return true;
    fs::create_directories(parent, ec);
    return !ec;
}

inline std::string getTimestamp(const char* fmt = "%Y-%m-%d %H:%M:%S") {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    struct tm tm_buf;
#if defined(_WIN32)
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::strftime(buf, sizeof(buf), fmt, &tm_buf);
    return std::string(buf);
}

inline bool openLogFile(std::ofstream& f, std::ios_base::openmode mode = std::ios::app) {
    std::string path = getLogPath();
    if (!ensureLogDirectoryExists(path))
        return false;
    f.open(path, mode);
    return f.good();
}

#if defined(__APPLE__)
static void paletteLogBundleAnchor() {}

inline std::string getPluginBundleRootPath() {
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&paletteLogBundleAnchor), &info) == 0 || !info.dli_fname)
        return "";
    std::string p = info.dli_fname;
    char resolved[PATH_MAX];
    if (realpath(p.c_str(), resolved))
        p = resolved;
    const char* marker = ".ofx.bundle";
    size_t pos = p.find(marker);
    if (pos == std::string::npos)
        return "";
    return p.substr(0, pos + std::strlen(marker));
}
#elif defined(_WIN32)
inline std::string getPluginBundleRootPath() {
    HMODULE mod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&getPluginBundleRootPath), &mod)
        || !mod)
        return "";
    wchar_t wpath[MAX_PATH];
    const DWORD n = GetModuleFileNameW(mod, wpath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return "";
    int needed = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1)
        return "";
    std::string p(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, p.data(), needed, nullptr, nullptr);
    const char* marker = ".ofx.bundle";
    size_t pos = p.find(marker);
    if (pos == std::string::npos)
        return "";
    return p.substr(0, pos + std::strlen(marker));
}
#else
inline std::string getPluginBundleRootPath() {
    return "";
}
#endif

inline void writeSessionStart(const std::string& pluginName,
    const std::string& versionStr,
    const std::string& hostName,
    const std::string& hostLabel,
    const std::string& hostVersion,
    const std::string& bundlePath) {
    std::lock_guard<std::mutex> lock(getLogMutex());
    std::ofstream f;
    if (!openLogFile(f, std::ios::out))
        return;

    std::string hostDisplay = hostLabel.empty() ? hostName : hostLabel;
    if (hostDisplay.empty())
        hostDisplay = "unknown";
    if (!hostVersion.empty())
        hostDisplay += " " + hostVersion;

    f << "------------------------------------------------------------\n";
    f << pluginName << " " << versionStr << "\n";
    f << getTimestamp("%a %b %d %H:%M:%S %Y") << "\n";
    f << "Host: " << hostDisplay << "\n";
    if (!bundlePath.empty())
        f << "Bundle: " << sanitizePathForLog(bundlePath) << "\n";
    f << "------------------------------------------------------------\n";
    f.flush();
}

inline void writeErrorLine(const std::string& message) {
    std::lock_guard<std::mutex> lock(getLogMutex());
    std::ofstream f;
    if (!openLogFile(f))
        return;
    f << getTimestamp() << " [error] " << message << "\n";
    f.flush();
}

inline void writeInfoLine(const std::string& message) {
    std::lock_guard<std::mutex> lock(getLogMutex());
    std::ofstream f;
    if (!openLogFile(f))
        return;
    f << getTimestamp() << " [info] " << message << "\n";
    f.flush();
}

} // namespace LSPPaletteLog

#define LSP_PALETTE_LOG_ERROR(msg) LSPPaletteLog::writeErrorLine(std::string(msg))
#define LSP_PALETTE_LOG_SESSION_START(n, v, hn, hl, hver, bundle) \
    LSPPaletteLog::writeSessionStart(std::string(n), std::string(v), std::string(hn), std::string(hl), std::string(hver), std::string(bundle))
