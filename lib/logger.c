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
#include <errno.h>

#include <snowflake/logger.h>
#include <snowflake/platform.h>
#include <string.h>
#include "../cpp/logger/SFLoggerCWrapper.h"

static struct {
    void *udata;
    log_LockFn lock;
    FILE *fp;
    int level;
    int quiet;
    const char *path;
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

int log_get_level()
{
    return L.level;
}

void log_set_level(int level) {
    L.level = level;
}


void log_set_quiet(int enable) {
    L.quiet = enable ? 1 : 0;
}

void log_log(int level, const char *file, int line, const char *ns,
             const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (getExternalLogger()){
      // va_list can only be consumed once. Make a copy so we can use it again
      va_list copy;
      va_copy(copy, args);

      // Add the line (since logLineVA doesn't take line)
      char linedFmt[strlen(fmt) + 10 /*max num of digits for line*/ + 3];
      sprintf(linedFmt, "%d: %s", line, fmt);

      externalLogger_logLineVA((SF_LOG_LEVEL) level,
                               sf_filename_from_path(file), linedFmt, copy);
      va_end(copy);
    }

    log_log_va_list(level, file, line, ns, fmt, args);
    va_end(args);
}


void
log_log_va_list(int level, const char *file, int line, const char *ns,
                const char *fmt, va_list args) {
    if (level < L.level) {
        return;
    }

    char tsbuf[50];    /* timestamp buffer*/
    sf_log_timestamp(tsbuf, sizeof(tsbuf));

    char *basename = sf_filename_from_path(file);

    /* Acquire lock */
    lock();

    /* Log to stderr */
    if (!L.quiet) {
#ifdef LOG_USE_COLOR
        fprintf(
            stderr, SF_LOG_TIMESTAMP_FORMAT_COLOR,
            tsbuf, level_colors[level], level_names[level], ns, basename,
            line);
#else
        fprintf(
            stderr, SF_LOG_TIMESTAMP_FORMAT,
             buf, level_names[level], namespace, basename, line);
#endif
        // va_list can only be consumed once. Make a copy here in case both
        // console and file logging are turned on.
        va_list copy;
        va_copy(copy, args);
        vfprintf(stderr, fmt, copy);
        va_end(copy);
        fprintf(stderr, "\n");
        fflush(stderr);
    }

    /* Log to file */
    /* Delay the log file creation to when there is log needs to write
     * to avoid empty log files.
     */
    if (!(L.fp) && L.path)
    {
        L.fp = fopen(L.path, "w+");
        if (!(L.fp))
        {
            fprintf(stderr,
                "Error opening file from file path: %s\nError code: %s\n",
                L.path, strerror(errno));
            L.path = NULL;
        }
    }

    if (L.fp) {
        fprintf(
            L.fp, SF_LOG_TIMESTAMP_FORMAT,
            tsbuf, level_names[level], ns, basename, line);
        vfprintf(L.fp, fmt, args);
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

void log_set_path(const char *path) {
    L.path = path;
}

void log_close() {
    /* Acquire lock */
    lock();

    if (L.fp)
    {
        fclose(L.fp);
        L.fp = NULL;
    }
    L.path = NULL;

    /* Release lock */
    unlock();
}
