/*
 * Copyright (c) 2017 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include <snowflake/logger.h>
#include <snowflake/platform.h>
#include <string.h>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/time.h>
#endif

static struct {
    void *udata;
    log_LockFn lock;
    FILE *fp;
    int level;
    int quiet;
} L;


static const char *level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
    "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};
#endif


static void lock(void) {
    if (L.lock) {
        L.lock(L.udata, 1);
    }
}


static void unlock(void) {
    if (L.lock) {
        L.lock(L.udata, 0);
    }
}


void log_set_udata(void *udata) {
    L.udata = udata;
}


void log_set_lock(log_LockFn fn) {
    L.lock = fn;
}


void log_set_fp(FILE *fp) {
    L.fp = fp;
}


void log_set_level(int level) {
    L.level = level;
}


void log_set_quiet(int enable) {
    L.quiet = enable ? 1 : 0;
}


void log_log(int level, const char *file, int line, const char *ns, const char *fmt, ...) {
    if (level < L.level) {
        return;
    }

    /* Get current time */
    struct timeval tmnow;
    gettimeofday(&tmnow, NULL);
    struct tm *lt = localtime(&tmnow.tv_sec);
    char tsbuf[50];    /* timestamp buffer*/
    char msec[10];    /* Microsecond buffer */

    char* basename = sf_filename_from_path(file);

    snprintf(msec, sizeof(msec), "%03d", (int) tmnow.tv_usec / 1000);

    /* Timestamp */
    tsbuf[strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';
    strcat(tsbuf, ".");
    strcat(tsbuf, msec);

    /* Acquire lock */
    lock();

    /* Log to stderr */
    if (!L.quiet) {
        va_list args;
#ifdef LOG_USE_COLOR
        fprintf(
            stderr, "%s %s%-5s\x1b[0m \x1b[90m%-5s %-16s %4d:\x1b[0m ",
            tsbuf, level_colors[level], level_names[level], ns, basename,
            line);
#else
        fprintf(
            stderr, "%s %-5s %-5 %-16s %4d: ",
             buf, level_names[level], namespace, basename, line);
#endif
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
        fflush(stderr);
    }

    /* Log to file */
    if (L.fp) {
        va_list args;
        fprintf(
            L.fp, "%s %-5s %-5s %-16s %4d: ",
            tsbuf, level_names[level], ns, basename, line);
        va_start(args, fmt);
        vfprintf(L.fp, fmt, args);
        va_end(args);
        fprintf(L.fp, "\n");
        fflush(L.fp);
    }

    /* Release lock */
    unlock();
}

SF_LOG_LEVEL log_from_str_to_level(const char *level_in_str) {
    if (level_in_str == NULL) {
        return SF_LOG_FATAL;
    }
    int idx = 0, last = 0;
    for (idx = 0, last = (int) SF_LOG_FATAL; idx <= last; ++idx) {
        size_t len = strlen(level_names[idx]);
        if (sf_strncasecmp(level_names[idx], level_in_str, len) == 0) {
            return (SF_LOG_LEVEL) idx;
        }
    }
    return SF_LOG_FATAL;
}

