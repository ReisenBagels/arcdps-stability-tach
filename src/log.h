#pragma once
// Tiny thread-safe file logger. Writes addons/arcdps/stability_tach/debug.log and rolls it to debug.log.1
// when it passes 5 MB (the previous .1 file is replaced), so disk use is capped at ~10 MB.
namespace Log {
    void setEnabled(bool on);      // opens / closes the file
    bool enabled();
    void write(const char* fmt, ...);   // printf-style; adds a timestamp and newline. No-op when disabled.
}
