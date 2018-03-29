/*
* Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
*/

#include <snowflake/platform.h>
#include <snowflake/basic_types.h>

#ifndef _WIN32
#include <regex.h>
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


int STDCALL _thread_init(SF_THREAD_HANDLE *thread, void *(*proc)(void *), void *arg) {
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

int STDCALL _cond_wait(SF_CONDITION_HANDLE *cond, SF_CRITICAL_SECTION_HANDLE *crit) {
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

sf_bool STDCALL _is_put_get_command(char* sql_text)
{
#ifdef _WIN32
    return SF_BOOLEAN_FALSE;
#else
    //TODO better handle regex for detecting put and get command
    regex_t put_get_regex;
    // On MacOS seems \s to match white space character did not work. Change to '[ ]' for now
    //TODO maybe regex compilation should be moved to static variable so that no recompilation needed
    regcomp(&put_get_regex, "^([ ]*\\/*.*\\/*[ ]*)*(put|get)[ ]+", REG_ICASE | REG_EXTENDED);

    int res;
    res = regexec(&put_get_regex, sql_text, 0, NULL, 0);

    regfree(&put_get_regex);

    return res == 0 ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
#endif
}

