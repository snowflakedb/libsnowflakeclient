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
#include <assert.h>
#include "memory.h"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/types.h>
#endif

static struct {
    void *udata;
    log_LockFn lock;
    FILE *fp;
    int level;
    int quiet;
    const char *path;
} L;


static const char *level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "DEFAULT"
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

#ifdef _WIN32
int sf_mkdir_perm(const char* path, size_t mode) {
  // mode is ignored on windows
  return _mkdir(path);
#else
int sf_mkdir_perm(const char* path, mode_t mode) {
  return mkdir(path, mode);
#endif
}

/**
 * Make a directory with the mode
 * @param file_path directory name
 * @param mode mode
 * @return 0 if success otherwise -1
 */
static int mkpath(char* file_path) {
  assert(file_path && *file_path);
  char* p;
  for (p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
    *p = '\0';
    if (sf_mkdir_perm(file_path, 0700) == -1) {
      if (errno != EEXIST) {
        *p = '/';
        return -1;
      }
    }
    *p = '/';
  }
  return 0;
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
    log_log_va_list(level, file, line, ns, fmt, args);
    va_end(args);
}


void
log_log_va_list(int level, const char *file, int line, const char *ns,
                const char *fmt, va_list args) {
    if (level < L.level) {
        return;
    }

    char strerr_buf[SF_ERROR_BUFSIZE];
    char tsbuf[50];    /* timestamp buffer*/
    sf_log_timestamp(tsbuf, sizeof(tsbuf));

    char *basename = sf_filename_from_path(file);

    /* Acquire lock */
    lock();

    /* Log to stderr */
    if (!L.quiet) {
#ifdef LOG_USE_COLOR
        sf_fprintf(
            stderr, SF_LOG_TIMESTAMP_FORMAT_COLOR,
            tsbuf, level_colors[level], level_names[level], ns, basename,
            line);
#else
        sf_fprintf(
            stderr, SF_LOG_TIMESTAMP_FORMAT,
             buf, level_names[level], namespace, basename, line);
#endif
        // va_list can only be consumed once. Make a copy here in case both
        // console and file logging are turned on.
        va_list copy;
        va_copy(copy, args);
        log_masked_va_list(stderr, fmt, copy);
        va_end(copy);
        sf_fprintf(stderr, "\n");
        fflush(stderr);
    }

    /* Log to file */
    /* Delay the log file creation to when there is log needs to write
     * to avoid empty log files.
     */
    if (!(L.fp) && L.path)
    {
        size_t log_path_size = strlen(L.path) + 1;
        char* log_path = (char*)SF_CALLOC(1, log_path_size);
        sf_strcpy(log_path, log_path_size, L.path);
        // Create log file path (if it doesn't exist)
        if (mkpath(log_path) == -1)
        {
            char* str_error = sf_strerror_s(errno, strerr_buf, sizeof(strerr_buf));
            sf_fprintf(stderr, "Error creating log directory. Error code: %s\n", str_error);
        }
        else if (sf_fopen(&L.fp, L.path, "w+") == NULL)
        {
            char* str_error = sf_strerror_s(errno, strerr_buf, sizeof(strerr_buf));
            sf_fprintf(stderr,
                "Error opening file from file path: %s\nError code: %s\n",
                L.path, str_error);
            L.path = NULL;
        }
        SF_FREE(log_path);
    }

    if (L.fp) {
        sf_fprintf(
            L.fp, SF_LOG_TIMESTAMP_FORMAT,
            tsbuf, level_names[level], ns, basename, line);
        log_masked_va_list(L.fp, fmt, args);
        sf_fprintf(L.fp, "\n");
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
    for (idx = 0, last = (int) SF_LOG_DEFAULT; idx <= last; ++idx) {
        size_t len = strlen(level_names[idx]);
        if (sf_strncasecmp(level_names[idx], level_in_str, len) == 0) {
            return (SF_LOG_LEVEL) idx;
        }
    }
    return SF_LOG_FATAL;
}

const char* log_from_level_to_str(SF_LOG_LEVEL level) {
    return level_names[level];
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
