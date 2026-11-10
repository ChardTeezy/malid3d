// malid3d - logging. Phone-only testing means the log file is our debugger,
// so everything important must end up here.
#pragma once

namespace malid3d {

// printf-style; a newline is appended. Thread-safe. Flushes on every line so
// the log survives a crash.
void mlog(const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(gnu_printf, 1, 2)))
#endif
    ;

}  // namespace malid3d
