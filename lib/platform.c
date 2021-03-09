/*
* Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
*/

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <snowflake/platform.h>
#include <snowflake/basic_types.h>
#include "client_int.h"

#if defined(__linux__) || defined(__APPLE__)

#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif


#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#ifndef _WIN32

#include <regex.h>
#include <ctype.h>

#endif

#ifdef _WIN32
#include <stdint.h>

// gmttime(), localtime() don't support negative epoch on Windows.
// Take the implementation from ODBC/DataConversion.cpp

// Seconds in a normal day
#define SECONDS_IN_DAY        ((int64_t)(24 * 60 * 60))
// Seconds in a normal, non-leap year
#define SECONDS_IN_YEAR       (365 * SECONDS_IN_DAY)
// Compute the number of seconds since year zero before the given year
#define SECONDS_BEFORE_YEAR(year) \
    (SECONDS_IN_YEAR * year + \
    /* Adjust for the number of leap years */ \
    SECONDS_IN_DAY * ((year + 3) / 4 - (year + 99) / 100 + (year + 399) / 400))
// Offset of UNIX EPOCH vs zero-years
#define UNIX_EPOCH_OFFSET     SECONDS_BEFORE_YEAR(1970)

// SystemTimeToTzSpecificLocalTime() doesn't work well for early years like 1600.
// Since no daylight-saving before 1900, use timezone offset directly to avoid
// incorrect result from SystemTimeToTzSpecificLocalTime().
#define NO_DAYLIGHT_OFFSET    (SECONDS_BEFORE_YEAR(1900) - UNIX_EPOCH_OFFSET)
// We observe that 400/100/4/1 year periods have fixed numbers of seconds/days
#define SEC_IN_Y400           (SECONDS_IN_DAY * (400 * 365 + 97))
#define SEC_IN_Y100           (SECONDS_IN_DAY * (100 * 365 + 24))
#define SEC_IN_Y4             (SECONDS_IN_DAY * (4 * 365 + 1))

// For all computations below, we want to always have a positive time value.
// To do that, we'll offset negative values by some multiple of 400 years.
// Note, 400 years is a nice number, as it is a cycle in "leap" years,
// but also has an exact multiple of 7 days, as ((400*365+100-4+1) % 7) == 0,
// so it doesn't influence the day-of-the-week calculation.
//
// Compute a safe number of years to add (half of sf::sb8 range of seconds).
#define NEGATIVE_NORMALIZATION_400_YEARS  (INT64_MAX / (2 * SEC_IN_Y400))
// Compute the matching number of seconds and years
#define NEGATIVE_NORMALIZATION_SECONDS    (NEGATIVE_NORMALIZATION_400_YEARS * SEC_IN_Y400)
#define NEGATIVE_NORMALIZATION_YEARS      (NEGATIVE_NORMALIZATION_400_YEARS * 400)

/**
*  Number of cumulative days BEFORE a given month for non-leap and leap years
*/
static const int cumulMonthDays[2][12] = {
    {   // non-leap year
        0,
        31,  // +31 Jan
        59,  // +28 Feb
        90,  // +31 Mar
        120, // +30 Apr
        151, // +31 May
        181, // +30 Jun
        212, // +31 Jul
        243, // +31 Aug
        273, // +30 Sep
        304, // +31 Oct
        334, // +31 Nov
    },
    {   // leap-year
        0,
        31,  // +31 Jan
        60,  // +29 Feb
        91,  // +31 Mar
        121, // +30 Apr
        152, // +31 May
        182, // +30 Jun
        213, // +31 Jul
        244, // +31 Aug
        274, // +30 Sep
        305, // +31 Oct
        335, // +31 Nov
    }
};

BOOL isLeapYear(int year)
{
    // Hackish is-leap-year implementation from http://jsperf.com/find-leap-year/4
    return !(year & 3 || (year & 15 && !(year % 25)));
}

struct tm* sfchrono_gmtime(const time_t *timep, struct tm *tm)
{
    time_t t = *timep;

    // Move into "standard" (non-UNIX) calendar realm, where year 1 AD is number 1
    t += UNIX_EPOCH_OFFSET;

    // Normalize, and remember the value we added (only years matter)
    // Note, a single addition might not suffice, hence a loop
    // (should be at most 3 times).
    int64_t normalizationYearsDelta = 0;
    while (t < 0)
    {
        t += NEGATIVE_NORMALIZATION_SECONDS;
        normalizationYearsDelta += NEGATIVE_NORMALIZATION_YEARS;
    }

    // Compute the time of the day
    time_t secsInDay = t % SECONDS_IN_DAY;
    tm->tm_hour = (int)(secsInDay / 3600);
    secsInDay -= tm->tm_hour * 3600;
    tm->tm_min = (int)(secsInDay / 60);
    secsInDay -= tm->tm_min * 60;
    tm->tm_sec = (int)secsInDay;

    // Compute the day of the week
    int64_t day = t / SECONDS_IN_DAY;
    tm->tm_wday = (day + 6) % 7;

    // Compute the year.

    // Go over different periods, and compute how many of each a given time has,
    // reducing time on the way
    int64_t y100 = 0;
    int64_t y4 = 0;
    int64_t y1 = 0;
    int64_t y400 = t / SEC_IN_Y400;
    t -= y400 * SEC_IN_Y400;
    if (t >= SECONDS_IN_YEAR + SECONDS_IN_DAY)
    {
        t -= SECONDS_IN_DAY;  // Adjust for last 400*x year
        y100 = t / SEC_IN_Y100;
        t -= y100 * SEC_IN_Y100;
        if (t >= SECONDS_IN_YEAR)
        {
            t += SECONDS_IN_DAY;  // Adjust for last 100*x year
            y4 = t / SEC_IN_Y4;
            t -= y4 * SEC_IN_Y4;
            if (t >= SECONDS_IN_YEAR + SECONDS_IN_DAY)
            {
                t -= SECONDS_IN_DAY;  // Adjust for last 4*x year
                y1 = t / SECONDS_IN_YEAR;
                t -= y1 * SECONDS_IN_YEAR;
            }
        }
    }
    int64_t year = 400 * y400 + 100 * y100 + 4 * y4 + y1;
    tm->tm_year = (int)(year - 1900 - normalizationYearsDelta);

    // Compute day of the year
    int yday = (int)(t / SECONDS_IN_DAY);
    tm->tm_yday = yday;

    // Compute day and month
    int isLeap = isLeapYear(year);
    int m;
    // @TODO Could do (hard-coded) binary search here
    for (m = 11; yday < cumulMonthDays[isLeap][m]; --m)
    {
    }
    tm->tm_mon = m;
    tm->tm_mday = yday - cumulMonthDays[isLeap][m] + 1;

    // GMT is never daylight-saving
    tm->tm_isdst = 0;
    // tm->tm_gmtoff = 0; // error in Windows
    return tm;
}

struct tm* sfchrono_localtime(const time_t *timep, struct tm *tm)
{
  time_t t = *timep;
  char * tzptr = sf_getenv("TZ");

  // use TZ environemt variable setting for timestamp_tz
  if (tzptr && (strlen(tzptr) >= 3) && (strncmp(tzptr, "UTC", 3) == 0))
  {
      t -= _timezone;
      sfchrono_gmtime(&t, tm);
  }
  // use system timezone offset when daylight-saving is not available
  else if(t < NO_DAYLIGHT_OFFSET)
  {
      TIME_ZONE_INFORMATION tzInfo;
      GetTimeZoneInformation(&tzInfo);
      t -= tzInfo.Bias * 60;
      sfchrono_gmtime(&t, tm);
  }
  else
  {
      sfchrono_gmtime(&t, tm);
      SYSTEMTIME gmTime, localTime;
      gmTime.wYear = tm->tm_year + 1900;
      gmTime.wMonth = tm->tm_mon + 1;
      gmTime.wDay = tm->tm_mday;
      gmTime.wHour = tm->tm_hour;
      gmTime.wMinute = tm->tm_min;
      gmTime.wSecond = tm->tm_sec;
      gmTime.wMilliseconds = 0;

      if (!SystemTimeToTzSpecificLocalTime(NULL, &gmTime, &localTime))
      {
        return NULL;
      }
      tm->tm_year = localTime.wYear - 1900;
      tm->tm_mon = localTime.wMonth - 1;
      tm->tm_mday = localTime.wDay;
      tm->tm_hour = localTime.wHour;
      tm->tm_min = localTime.wMinute;
      tm->tm_sec = localTime.wSecond;
  }
  return tm;
}
#endif

struct tm *STDCALL sf_gmtime(const time_t *timep, struct tm *result) {
#ifdef _WIN32
  return sfchrono_gmtime(timep, result);
#else
    return gmtime_r(timep, result);
#endif
}

struct tm *STDCALL sf_localtime(const time_t *timep, struct tm *result) {
#ifdef _WIN32
  return sfchrono_localtime(timep, result);
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
    CloseHandle(thread);
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
  // TODO use some library to parse put get command in windows
  char *it;
  sf_bool in_comment_block = SF_BOOLEAN_FALSE;
  sf_bool is_put_get_command = SF_BOOLEAN_FALSE;
  for (it = sql_text; it != '\0'; it++)
  {
    if (in_comment_block)
    {
      if (*it == '*' && * (it + 1) == '/')
      {
        it++;
        in_comment_block = SF_BOOLEAN_FALSE;
      }
    }
    else
    {
      if (*it == ' ')
      {
        // do nothing
      }
      else if (*it == '/' && * (it + 1) == '*')
      {
        in_comment_block = SF_BOOLEAN_TRUE;
      }
      else if (*it == 'p' || *it == 'P')
      {
        if ((*(it + 1) == 'u' || *(it + 1) == 'U')
          && (*(it + 2) == 't' || *(it + 2) == 'T'))
        {
          return SF_BOOLEAN_TRUE;
        }
        else
        {
          return SF_BOOLEAN_FALSE;
        }
      }
      else if (*it == 'g' || *it == 'G')
      {
        if ((*(it + 1) == 'e' || *(it + 1) == 'E')
          && (*(it + 2) == 't' || *(it + 2) == 'T'))
        {
          return SF_BOOLEAN_TRUE;
        }
        else
        {
          return SF_BOOLEAN_FALSE;
        }
      }
      else
      {
        return SF_BOOLEAN_FALSE;
      }
    }
  }
  return SF_BOOLEAN_FALSE;

#else
    //TODO better handle regex for detecting put and get command
    regex_t put_get_regex;
    // On MacOS seems \s to match white space character did not work. Change to '[ ]' for now
    //TODO maybe regex compilation should be moved to static variable so that no recompilation needed
    regcomp(&put_get_regex, "^([ ]*\\/\\*.*\\*\\/[ ]*)*([ ]*)*(put|get)[ ]+",
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
void STDCALL sf_os_version(char *ret, size_t size) {
    sb_strcpy(ret, size, "0.0.0"); /* unknown version */
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
        sb_strcpy(ret, size, envbuf.release);
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

    sb_sprintf(ret, size, "%d.%d-%s",
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
 * Filename from the path.
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
    return (char *) path;
}

/**
 * Timestamp string for log
 *
 * @param tsbuf output timestamp string buffer
 */
void STDCALL sf_log_timestamp(char *tsbuf, size_t tsbufsize) {
#if defined(__linux__) || defined(__APPLE__)
    /* Get current time */
    struct timeval tmnow;
    gettimeofday(&tmnow, NULL);
    struct tm *lt = gmtime(&tmnow.tv_sec);
    char msec[10];    /* Microsecond buffer */

    sb_sprintf(msec, sizeof(msec), "%03d", (int) tmnow.tv_usec / 1000);

    /* Timestamp */
    strftime(tsbuf, tsbufsize, "%Y-%m-%d %H:%M:%S", lt);
    sb_strcat(tsbuf, tsbufsize, ".");
    sb_strcat(tsbuf, tsbufsize, msec);
#else /* Windows */
    SYSTEMTIME t;
    // Get the system time, which is expressed in UTC
    GetSystemTime(&t);
    sb_sprintf(tsbuf, tsbufsize, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        t.wYear, t.wMonth, t.wDay,
        t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
#endif
}

int STDCALL sf_create_directory_if_not_exists(const char * directoryName)
{
#ifdef _WIN32
  return !(CreateDirectory(directoryName, NULL)
    || ERROR_ALREADY_EXISTS == GetLastError());
#else
  // read, write, execute by user and group
  struct stat st = {0};
  return stat(directoryName, &st) == -1 ?
      mkdir(directoryName, S_IRWXU | S_IRWXG) : 0;
#endif
}

int STDCALL sf_is_directory_exist(const char * directoryName)
{
#ifdef _WIN32
  DWORD dwAttrib = GetFileAttributes(directoryName);
  return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
#else
  struct stat sb;
  return (stat(directoryName, &sb) == 0 && S_ISDIR(sb.st_mode));
#endif
}

int STDCALL sf_create_directory_if_not_exists_recursive(const char * directoryName)
{
  char fullPath[MAX_PATH + 1] = {0};
  char partPath[MAX_PATH + 1] = {0};
  char pathSepStr[2] = {PATH_SEP, '\0'};
  char* token = NULL;
  char *next = NULL;

  sb_strcpy(fullPath, sizeof(fullPath), directoryName);

#ifdef _WIN32
  token = strtok_s(fullPath, pathSepStr, &next);
  while (token)
  {
    sb_strcat(partPath, sizeof(partPath), token);
    token = strtok_s(NULL, pathSepStr, &next);
#else
  if ('/' == fullPath[0])
  {
    sb_strcat(partPath, sizeof(partPath), "/");
  }
  token = strtok_r(fullPath, pathSepStr, &next);
  while (token)
  {
    sb_strcat(partPath, sizeof(partPath), token);
    token = strtok_r(NULL, pathSepStr, &next);
#endif
    sb_strcat(partPath, sizeof(partPath), pathSepStr);
    if (sf_is_directory_exist(partPath))
    {
      continue;
    }
    int ret = sf_create_directory_if_not_exists(partPath);
    if (ret != 0)
    {
      return ret;
    }
  }

  return 0;
}

int STDCALL sf_delete_directory_if_exists(const char * directoryName)
{
#ifdef _WIN32
  char rmCmd[500];
  sb_strcpy(rmCmd, sizeof(rmCmd), "rd /s /q ");
  sb_strcat(rmCmd, sizeof(rmCmd), directoryName);
  return system(rmCmd);
#else
  char rmCmd[500];
  sb_strcpy(rmCmd, sizeof(rmCmd), "rm -rf ");
  sb_strcat(rmCmd, sizeof(rmCmd), directoryName);
  return system(rmCmd);
#endif
}

void STDCALL sf_get_tmp_dir(char * tmpDir)
{
#ifdef _WIN32
  GetTempPath(100, tmpDir);
#else
  const char * tmpEnv = getenv("TMP") ? getenv("TMP") : getenv("TEMP");

  if (!tmpEnv)
  {
    sb_strncpy(tmpDir, 100, "/tmp/", sizeof("/tmp/"));
  }
  else
  {
    sb_strncpy(tmpDir, 100, tmpEnv, strlen(tmpEnv));
    size_t oldLen = strlen(tmpDir);
    tmpDir[oldLen] = PATH_SEP;
    tmpDir[oldLen+1] = '\0';
  }
#endif
}

/*Returns a unique temporary directory based uuid string
 * tmpDir: If the input is empty, use the default value of
 *         /tmp/snowflakeTmp_<username>/<uuid-string>/
 *         otherwise <temp dir>/<uuid-string>/
 * And tmpDir can hold upto Max path allowed on respective platforms.
 */
void STDCALL sf_get_uniq_tmp_dir(char * tmpDir)
{
  char uuid_cstr[37]; // 36 byte uuid plus null.
  char username[1024];
  uuid4_generate(uuid_cstr);
  sf_get_username(username, sizeof(username));
  char pathSepStr[2] = {0};
  pathSepStr[0] = PATH_SEP;
  if (strlen(tmpDir) == 0)
  {
    sf_get_tmp_dir(tmpDir);
    if (tmpDir[strlen(tmpDir) - 1] != PATH_SEP)
    {
      sb_strcat(tmpDir, MAX_PATH, pathSepStr);
    }
    sb_strcat(tmpDir, MAX_PATH, "snowflakeTmp_");
    sb_strcat(tmpDir, MAX_PATH, username);
    sb_strcat(tmpDir, MAX_PATH, pathSepStr);
  }
  else if (tmpDir[strlen(tmpDir) - 1] != PATH_SEP)
  {
    sb_strcat(tmpDir, MAX_PATH, pathSepStr);
  }
  sb_strcat(tmpDir, MAX_PATH, uuid_cstr);
  sb_strcat(tmpDir, MAX_PATH, pathSepStr);
  sf_create_directory_if_not_exists_recursive(tmpDir);
}

void STDCALL sf_get_username(char * username, int bufLen)
{
  if ((!username) || (bufLen <= 0))
  {
    return;
  }
#ifdef _WIN32
  // GetUserNameW returns non-zero values on success and zero on failure
  if (GetUserName(username, &bufLen) == 0)
  {
    *username = '\0';
  }
#else
  struct passwd pw;           // The password object which holds username
  struct passwd* pwPtr;       // A Temp pointer to pw; useless but required by getpwuid_r
  char pwBuf[1024];
  int pwBufLen = sizeof(pwBuf);

  // Get the user id
  uid_t userId = getuid();

  // Get passwd object from user ID
  if (0 != getpwuid_r(userId, &pw, pwBuf, pwBufLen, &pwPtr))
  {
    *username = '\0';
  }
  else
  {
    sb_strcpy(username, bufLen, pw.pw_name);
  }
#endif
}

void STDCALL sf_delete_uniq_dir_if_exists(const char *tmpfile)
{
    size_t i=0;
    size_t len=0; 
    char fpath[MAX_PATH];
    sb_strcpy(fpath, sizeof(fpath), tmpfile);     
    len=strlen(fpath);
    for(i=len ; i > 0 ; i--)
    {   
        if(fpath[i] == PATH_SEP)
        {
            fpath[i]=0;
            break;
        }
    }   
    sf_delete_directory_if_exists(fpath);
}

