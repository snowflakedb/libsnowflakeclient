/*
* Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
*/

#include <snowflake/platform.h>
#include <snowflake/basic_types.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#ifndef _WIN32

#include <regex.h>
#include <string.h>
#include <ctype.h>

#endif

struct tm *STDCALL sf_gmtime(const time_t *timep, struct tm *result) {
#ifdef _WIN32
    errno_t err = gmtime_s(result, timep);
    if (err) {
        return NULL;
    }
    return result;
#else
    return gmtime_r(timep, result);
#endif
}

struct tm *STDCALL sf_localtime(const time_t *timep, struct tm *result) {
#ifdef _WIN32
    errno_t err = localtime_s(result, timep);
    if (err) {
        return NULL;
    }
    return result;
#else
    return localtime_r(timep, result);
#endif
}

void STDCALL sf_tzset(void) {
#ifdef _WIN32
    _tzset();
#else
    tzset();
#endif
}

int STDCALL sf_setenv(const char *name, const char *value) {
#ifdef _WIN32
    return _putenv_s(name, value);
#else
    return setenv(name, value, 1);
#endif
}

char *STDCALL sf_getenv(const char *name) {
#ifdef _WIN32
    return getenv(name);
#else
    return getenv(name);
#endif
}

int STDCALL sf_unsetenv(const char *name) {
#ifdef _WIN32
    return _putenv_s(name, "");
#else
    return unsetenv(name);
#endif
}

int STDCALL sf_mkdir(const char *path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}


int STDCALL
_thread_init(SF_THREAD_HANDLE *thread, void *(*proc)(void *), void *arg) {
#ifdef _WIN32
    *thread = CreateThread(
      NULL,                         // default security attributes
      0,                            // default stack size
      (LPTHREAD_START_ROUTINE)proc, // thread function name
      arg,                          // argument to thread function
      0,                            // default creation flag
      NULL                          // thread id
    );
    return 0;
#else
    return pthread_create(thread, NULL, proc, arg);
#endif
}

int STDCALL _thread_join(SF_THREAD_HANDLE thread) {
#ifdef _WIN32
    DWORD ret = WaitForSingleObject(thread, INFINITE);
    return ret == WAIT_OBJECT_0 ? 0 : 1;
#else
    return pthread_join(thread, NULL);
#endif
}

void STDCALL _thread_exit() {
#ifdef _WIN32
    ExitThread(0);
#else
    pthread_exit(NULL);
#endif
}

int STDCALL _cond_init(SF_CONDITION_HANDLE *cond) {
#ifdef _WIN32
    InitializeConditionVariable(cond);
    return 0;
#else
    return pthread_cond_init(cond, NULL);
#endif
}

int STDCALL _cond_broadcast(SF_CONDITION_HANDLE *cond) {
#ifdef _WIN32
    WakeAllConditionVariable(cond);
    return 0;
#else
    return pthread_cond_broadcast(cond);
#endif
}

int STDCALL _cond_signal(SF_CONDITION_HANDLE *cond) {
#ifdef _WIN32
    WakeConditionVariable(cond);
    return 0;
#else
    return pthread_cond_signal(cond);
#endif
}

int STDCALL
_cond_wait(SF_CONDITION_HANDLE *cond, SF_CRITICAL_SECTION_HANDLE *crit) {
#ifdef _WIN32
    BOOL ret = SleepConditionVariableCS(cond, crit, INFINITE);
    return ret ? 0 : 1;
#else
    return pthread_cond_wait(cond, crit);
#endif
}

int STDCALL _cond_term(SF_CONDITION_HANDLE *cond) {
#ifdef _WIN32
    // nop
    return 0;
#else
    return pthread_cond_destroy(cond);
#endif
}

int STDCALL _critical_section_init(SF_CRITICAL_SECTION_HANDLE *crit) {
#ifdef _WIN32
    InitializeCriticalSection(crit);
    return 0;
#else
    return pthread_mutex_init(crit, NULL);
#endif
}

int STDCALL _critical_section_lock(SF_CRITICAL_SECTION_HANDLE *crit) {
#ifdef _WIN32
    EnterCriticalSection(crit);
    return 0;
#else
    return pthread_mutex_lock(crit);
#endif
}

int STDCALL _critical_section_unlock(SF_CRITICAL_SECTION_HANDLE *crit) {
#ifdef _WIN32
    LeaveCriticalSection(crit);
    return 0;
#else
    return pthread_mutex_unlock(crit);
#endif
}

int STDCALL _critical_section_term(SF_CRITICAL_SECTION_HANDLE *crit) {
#ifdef _WIN32
    DeleteCriticalSection(crit);
    return 0;
#else
    return pthread_mutex_destroy(crit);
#endif
}

int STDCALL _rwlock_init(SF_RWLOCK_HANDLE *lock) {
#ifdef _WIN32
    InitializeSRWLock(lock);
    return 0;
#else
    return pthread_rwlock_init(lock, NULL);
#endif
}

int STDCALL _rwlock_rdlock(SF_RWLOCK_HANDLE *lock) {
#ifdef _WIN32
    AcquireSRWLockShared(lock);
    return 0;
#else
    return pthread_rwlock_rdlock(lock);
#endif
}

int STDCALL _rwlock_rdunlock(SF_RWLOCK_HANDLE *lock) {
#ifdef _WIN32
    ReleaseSRWLockShared(lock);
    return 0;
#else
    return pthread_rwlock_unlock(lock);
#endif
}

int STDCALL _rwlock_wrlock(SF_RWLOCK_HANDLE *lock) {
#ifdef _WIN32
    AcquireSRWLockExclusive(lock);
    return 0;
#else
    return pthread_rwlock_wrlock(lock);
#endif
}

int STDCALL _rwlock_wrunlock(SF_RWLOCK_HANDLE *lock) {
#ifdef _WIN32
    ReleaseSRWLockExclusive(lock);
    return 0;
#else
    return pthread_rwlock_unlock(lock);
#endif
}

int STDCALL _rwlock_term(SF_RWLOCK_HANDLE *lock) {
#ifdef _WIN32
    // nop
    return 0;
#else
    return pthread_rwlock_destroy(lock);
#endif
}

int STDCALL _mutex_init(SF_MUTEX_HANDLE *lock) {
#ifdef _WIN32
    *lock = CreateMutex(
        NULL,  // default security attribute
        FALSE, // initially not owned
        NULL   // unnamed mutext
    );
    return 0;
#else
    return pthread_mutex_init(lock, NULL);
#endif
}

int STDCALL _mutex_lock(SF_MUTEX_HANDLE *lock) {
#ifdef _WIN32
    DWORD ret = WaitForSingleObject(*lock, INFINITE);
    return ret == WAIT_OBJECT_0 ? 0 : 1;
#else
    return pthread_mutex_lock(lock);
#endif
}

int STDCALL _mutex_unlock(SF_MUTEX_HANDLE *lock) {
#ifdef _WIN32
    ReleaseMutex(*lock);
    return 0;
#else
    return pthread_mutex_unlock(lock);
#endif
}

int STDCALL _mutex_term(SF_MUTEX_HANDLE *lock) {
#ifdef _WIN32
    CloseHandle(*lock);
    return 0;
#else
    return pthread_mutex_destroy(lock);
#endif
}

sf_bool STDCALL _is_put_get_command(char *sql_text) {
#ifdef _WIN32
    return SF_BOOLEAN_FALSE;
#else
    //TODO better handle regex for detecting put and get command
    regex_t put_get_regex;
    // On MacOS seems \s to match white space character did not work. Change to '[ ]' for now
    //TODO maybe regex compilation should be moved to static variable so that no recompilation needed
    regcomp(&put_get_regex, "^([ ]*\\/*.*\\/*[ ]*)*(put|get)[ ]+",
            REG_ICASE | REG_EXTENDED);

    int res;
    res = regexec(&put_get_regex, sql_text, 0, NULL, 0);

    regfree(&put_get_regex);

    return res == 0 ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
#endif
}

/**
 * Get Operating System name
 */
const char *STDCALL sf_os_name() {
#ifdef __APPLE__
    return "Darwin";
#elif __linux__
    return "Linux";
#elif _WIN32
    // for both x64 and x86
    return "Windows";
#else
    return "Unknown";
#endif
}

/**
 * Get Operating System version
 */
void STDCALL sf_os_version(char *ret) {
    strcpy(ret, "0.0.0"); /* unknown version */
#ifdef __APPLE__
    // Version  OS Name
    //  17.x.x  macOS 10.13.x High Sierra
    //  16.x.x  macOS 10.12.x Sierra
    //  15.x.x  OS X  10.11.x El Capitan
    size_t len = 64; /* hard coded */
    sysctlbyname("kern.osrelease", ret, &len, NULL, 0);
#elif __linux__
    // Kernel version
    struct utsname envbuf;
    if (uname(&envbuf) == 0) {
        strcpy(ret, envbuf.release);
    }
#elif _WIN32
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms724833%28v=vs.85%29.aspx
    // Version  Actual Windows Version
    //    10.0  Windows 10.0/Server 2016
    //     6.3  Windows  8.1/Server 2012 R2
    //     6.2  Windows  8.0/Server 2012 R1
    //     6.1  Windows  7.0/Server 2008 R2
    //     6.0  Windows  Vist/Server 2008 R1 (Not supported)
    OSVERSIONINFOEX info;
    ZeroMemory(&info, sizeof(OSVERSIONINFOEX));
    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx((LPOSVERSIONINFO)&info);

    sprintf(ret, "%d.%d-%s",
        (int)info.dwMajorVersion,
        (int)info.dwMinorVersion,
#if _WIN64
        "x86_64"
#else
        "x86"
#endif // _WIN64
    );
#endif // Unknown Platform
}

/**
 * Compare strings case-insensitively
 * @param s1 source string
 * @param s2 target string
 * @param n number of maximum bytes to compare
 * @return 0 if identical otherwise different
 */
int STDCALL sf_strncasecmp(const char *s1, const char *s2, size_t n) {
    if (n == 0) {
        return 0;
    }

    while (n-- != 0 && tolower(*s1) == tolower(*s2)) {
        if (n == 0 || *s1 == '\0' || *s2 == '\0')
            break;
        s1++;
        s2++;
    }

    return tolower(*(unsigned char *) s1) - tolower(*(unsigned char *) s2);
}


/**
 * filename from the path.
 *
 * NOTE: no multibyte character set is supported. This is mainly used
 * for logging.
 *
 * @param path the full path
 * @return the pointer to the file name.
 */
char *STDCALL sf_filename_from_path(const char *path) {
    char *ret = strrchr(
        path,
#if defined(__linux__) || defined(__APPLE__)
        (int) '/'
#else
        (int)'\\'
#endif
    );
    if (ret != NULL) {
        return ret + 1;
    }
    return (char*)path;
}
