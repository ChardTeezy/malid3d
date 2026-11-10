#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace malid3d {

namespace {

const char kLogName[] = "malid3d.log";

struct LogState {
    CRITICAL_SECTION lock;
    FILE* file = nullptr;
    bool opened = false;
    LogState() { InitializeCriticalSection(&lock); }
};

LogState& state() {
    static LogState s;
    return s;
}

FILE* try_open(const char* dir) {
    char path[MAX_PATH * 2];
    size_t n = strlen(dir);
    if (n == 0 || n + sizeof(kLogName) + 2 > sizeof(path)) return nullptr;
    memcpy(path, dir, n);
    if (path[n - 1] != '\\' && path[n - 1] != '/') path[n++] = '\\';
    memcpy(path + n, kLogName, sizeof(kLogName));
    return fopen(path, "w");
}

// Order: $MALID3D_LOG_PATH, the game's exe folder, then the temp folder.
FILE* open_log() {
    char buf[MAX_PATH];

    DWORD n = GetEnvironmentVariableA("MALID3D_LOG_PATH", buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf)) {
        if (FILE* f = try_open(buf)) return f;
    }

    n = GetModuleFileNameA(nullptr, buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf)) {
        char* slash = strrchr(buf, '\\');
        if (!slash) slash = strrchr(buf, '/');
        if (slash) {
            *slash = '\0';
            if (FILE* f = try_open(buf)) return f;
        }
    }

    n = GetTempPathA(sizeof(buf), buf);
    if (n > 0 && n < sizeof(buf)) {
        if (FILE* f = try_open(buf)) return f;
    }
    return nullptr;
}

}  // namespace

void mlog(const char* fmt, ...) {
    LogState& s = state();
    EnterCriticalSection(&s.lock);

    if (!s.opened) {
        s.opened = true;
        s.file = open_log();
    }

    char line[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    if (s.file) {
        fputs(line, s.file);
        fputc('\n', s.file);
        fflush(s.file);
    }
    // Also visible in Wine's own debug output.
    OutputDebugStringA(line);

    LeaveCriticalSection(&s.lock);
}

}  // namespace malid3d
