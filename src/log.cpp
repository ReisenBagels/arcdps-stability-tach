#include "log.h"
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <mutex>

namespace {
constexpr long long kMaxBytes = 5ll * 1024 * 1024;
const char* kDir = "addons/arcdps/stability_tach";
const char* kPath = "addons/arcdps/stability_tach/debug.log";
const char* kOld = "addons/arcdps/stability_tach/debug.log.1";
std::mutex m;
FILE* f = nullptr;
long long bytes = 0;
std::atomic<bool> on{false};

void openLocked() {
    std::error_code ec;
    std::filesystem::create_directories(kDir, ec);
    f = std::fopen(kPath, "ab");
    if (f) { std::fseek(f, 0, SEEK_END); bytes = std::ftell(f); } else bytes = 0;
}
void closeLocked() { if (f) { std::fclose(f); f = nullptr; } }
void rollLocked() {
    closeLocked();
    std::error_code ec;
    std::filesystem::remove(kOld, ec);
    std::filesystem::rename(kPath, kOld, ec);
    openLocked();
}
}

namespace Log {
void setEnabled(bool enable) {
    std::lock_guard<std::mutex> l(m);
    on = enable;
    if (enable && !f) openLocked();
    if (!enable) closeLocked();
}
bool enabled() { return on.load(); }

void write(const char* fmt, ...) {
    if (!on.load()) return;
    std::lock_guard<std::mutex> l(m);
    if (!f) return;
    char buf[1100];
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    int ms = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    int n = (int)std::strftime(buf, 32, "%Y-%m-%d %H:%M:%S", &tmv);
    n += std::snprintf(buf + n, sizeof buf - n, ".%03d ", ms);
    va_list ap; va_start(ap, fmt);
    int w = std::vsnprintf(buf + n, sizeof buf - n - 2, fmt, ap);
    va_end(ap);
    if (w < 0) return;
    n += (w < (int)(sizeof buf - n - 2)) ? w : (int)(sizeof buf - n - 3);
    buf[n++] = '\n';
    std::fwrite(buf, 1, n, f);
    std::fflush(f);
    bytes += n;
    if (bytes >= kMaxBytes) rollLocked();
}
}
