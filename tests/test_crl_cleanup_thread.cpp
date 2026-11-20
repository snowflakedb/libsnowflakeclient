#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <curl/curl.h>
#include <openssl/x509.h>

#include "utils/test_setup.h"

/*
 * Tests for the CRL cache cleanup thread. A process that initializes curl and
 * then forks (e.g. a PHP-FPM master) must not hand its children a thread
 * handle they would join on exit, nor a cache mutex left locked.
 */

extern "C" {
struct Curl_easy;
void registerCRLCheck(struct Curl_easy *data, X509_STORE *ctx,
                      bool crl_advisory, bool crl_allow_no_crl,
                      bool crl_disk_caching, bool crl_memory_caching,
                      long crl_download_timeout, long crl_download_max_size);
void cleanupCertCRLCache(void);
void initCertCRL(void);
void termCertCRL(void);
int isCertCRLCleanupThreadRunning(void);
}

extern char **environ;

namespace {

std::string g_cache_dir;
pthread_t g_main_thread;
std::atomic<bool> g_main_thread_set(false);
std::atomic<int> g_getenv_calls(0);
std::atomic<bool> g_getenv_off_main_thread(false);

} // namespace

#ifdef __GLIBC__
#define SF_GETENV_NOEXCEPT noexcept
#else
#define SF_GETENV_NOEXCEPT
#endif

// Replaces libc getenv() for the statically linked library code, so calls
// from any thread other than the main one are recorded.
extern "C" char *getenv(const char *name) SF_GETENV_NOEXCEPT {
  ++g_getenv_calls;
  if (g_main_thread_set.load() && !pthread_equal(pthread_self(), g_main_thread)) {
    g_getenv_off_main_thread.store(true);
  }
  const size_t len = strlen(name);
  for (char **ep = environ; ep && *ep; ++ep) {
    if (strncmp(*ep, name, len) == 0 && (*ep)[len] == '=') {
      return *ep + len + 1;
    }
  }
  return nullptr;
}

namespace {

// Valid until 2100, so cleanup parses it on every pass and keeps it on disk.
const char kFreshCrlPem[] =
    "-----BEGIN X509 CRL-----\n"
    "MIGpMFACAQEwCgYIKoZIzj0EAwIwDzENMAsGA1UEAwwEdGVzdBcNMjYxMTE2MDk1\n"
    "MTQ1WhgPMjEwMDEwMTkwOTUxNDVaoA4wDDAKBgNVHRQEAwIBATAKBggqhkjOPQQD\n"
    "AgNJADBGAiEAifVf5StPiUav5d35vB+1YqS5QODBZ3uyEG99WikKoDUCIQDgDJuu\n"
    "jUKSs50PYWdN41lKslBnd5YZeDR/b7GgTUtgbQ==\n"
    "-----END X509 CRL-----\n";

volatile sig_atomic_t g_sigusr1_handled = 0;
CURL *g_child_curl = nullptr;
X509_STORE *g_child_store = nullptr;

// registerCRLCheck keeps both pointers, so they must outlive the process.
void register_store(CURL *curl, X509_STORE *store) {
  registerCRLCheck(static_cast<struct Curl_easy *>(curl), store,
                   false, false, true, true, 10, 0);
}

void register_crl_check() {
  register_store(curl_easy_init(), X509_STORE_new());
}

pid_t spawn_child(void (*body)()) {
  fflush(stdout);
  fflush(stderr);
  pid_t pid = fork();
  if (pid == 0) {
    // cmocka's handlers would otherwise jump back into the test runner inside
    // the child instead of letting it die with the signal.
    const int default_signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGSYS, SIGALRM};
    for (int sig : default_signals) {
      signal(sig, SIG_DFL);
    }
    alarm(10);
    body();
    _exit(99);
  }
  return pid;
}

int wait_child(pid_t pid) {
  int status = 0;
  if (pid <= 0 || waitpid(pid, &status, 0) != pid) {
    return -1;
  }
  return status;
}

void assert_exited_cleanly(int status) {
  assert_int_not_equal(status, -1);
  if (WIFSIGNALED(status)) {
    fail_msg("child was killed by signal %d", WTERMSIG(status));
  }
  assert_true(WIFEXITED(status));
  assert_int_equal(WEXITSTATUS(status), 0);
}

} // namespace

void test_fork_before_crl_use(void **unused) {
  SF_UNUSED(unused);
  assert_int_equal(isCertCRLCleanupThreadRunning(), 0);
  // The child runs the full teardown even when it sees an inherited thread.
  assert_exited_cleanly(wait_child(spawn_child([] {
    const int inherited_thread = isCertCRLCleanupThreadRunning();
    curl_global_cleanup();
    exit(inherited_thread ? 2 : 0);
  })));
}

// The cleanup thread must not call getenv(): it is not safe while another
// thread modifies the environment.
void test_cleanup_thread_does_not_read_environment(void **unused) {
  SF_UNUSED(unused);
  // Cleanup removes cache-named files that do not parse as a CRL.
  const std::string path = g_cache_dir + "/http___crl.example.com_junk";
  FILE *f = fopen(path.c_str(), "w");
  assert_non_null(f);
  fclose(f);

  const int calls_before = g_getenv_calls.load();
  register_crl_check();
  assert_true(g_getenv_calls.load() > calls_before);
  for (int i = 0; i < 1000 && access(path.c_str(), F_OK) == 0; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  assert_int_not_equal(access(path.c_str(), F_OK), 0);
  assert_false(g_getenv_off_main_thread.load());
}

void test_fork_after_crl_use(void **unused) {
  SF_UNUSED(unused);
  register_crl_check();
  assert_int_equal(isCertCRLCleanupThreadRunning(), 1);
  assert_exited_cleanly(wait_child(spawn_child([] {
    if (isCertCRLCleanupThreadRunning()) {
      _exit(3);
    }
    register_crl_check();
    if (!isCertCRLCleanupThreadRunning()) {
      _exit(4);
    }
    // No thread may start after teardown, until the next init.
    termCertCRL();
    register_crl_check();
    if (isCertCRLCleanupThreadRunning()) {
      _exit(5);
    }
    initCertCRL();
    register_crl_check();
    if (!isCertCRLCleanupThreadRunning()) {
      _exit(6);
    }
    // exit() runs the atexit teardown, which joins the new thread.
    exit(0);
  })));
}

void test_disabled_cleanup_not_retried(void **unused) {
  SF_UNUSED(unused);
  assert_exited_cleanly(wait_child(spawn_child([] {
    setenv("SF_CRL_CACHE_CLEANUP_INTERVAL", "0", 1);
    register_crl_check();
    if (isCertCRLCleanupThreadRunning()) {
      _exit(7);
    }
    setenv("SF_CRL_CACHE_CLEANUP_INTERVAL", "3600", 1);
    register_crl_check();
    _exit(isCertCRLCleanupThreadRunning() ? 8 : 0);
  })));
}

// The cleanup thread uses the settings read by the latest registration.
void test_cleanup_thread_uses_latest_settings(void **unused) {
  SF_UNUSED(unused);
  assert_exited_cleanly(wait_child(spawn_child([] {
    setenv("SF_CRL_CACHE_CLEANUP_INTERVAL", "1", 1);
    register_crl_check();
    char dir[] = "/tmp/crl_latest_XXXXXX";
    if (!mkdtemp(dir)) {
      _exit(11);
    }
    const std::string path = std::string(dir) + "/http___crl.example.com_junk";
    FILE *f = fopen(path.c_str(), "w");
    if (!f) {
      _exit(12);
    }
    fclose(f);
    setenv("SF_CRL_RESPONSE_CACHE_DIR", dir, 1);
    register_crl_check();
    for (int i = 0; i < 500 && access(path.c_str(), F_OK) == 0; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const bool removed = access(path.c_str(), F_OK) != 0;
    remove(path.c_str());
    rmdir(dir);
    _exit(removed ? 0 : 13);
  })));
}

// Signals sent to the process must not be handled on the cleanup thread.
void test_cleanup_thread_blocks_signals(void **unused) {
  SF_UNUSED(unused);
  assert_exited_cleanly(wait_child(spawn_child([] {
    signal(SIGUSR1, [](int) { g_sigusr1_handled = 1; });
    sigset_t usr1;
    sigemptyset(&usr1);
    sigaddset(&usr1, SIGUSR1);
    pthread_sigmask(SIG_UNBLOCK, &usr1, nullptr);
    register_crl_check();
    if (!isCertCRLCleanupThreadRunning()) {
      _exit(9);
    }
    // Only the cleanup thread could take the signal now.
    pthread_sigmask(SIG_BLOCK, &usr1, nullptr);
    kill(getpid(), SIGUSR1);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    sigset_t pending;
    sigpending(&pending);
    _exit(!g_sigusr1_handled && sigismember(&pending, SIGUSR1) ? 0 : 10);
  })));
}

void test_fork_while_registering(void **unused) {
  SF_UNUSED(unused);
  // Each new store grows the shared registry, which a child must not see
  // half-resized.
  std::vector<std::thread> registrars;
  for (int t = 0; t < 4; ++t) {
    registrars.emplace_back([] {
      for (int i = 0; i < 200; ++i) {
        register_crl_check();
      }
    });
  }

  std::vector<pid_t> children;
  for (int i = 0; i < 50; ++i) {
    // Created before fork(): a registrar may hold curl or OpenSSL locks that
    // no fork handler releases, so the child only calls registerCRLCheck.
    g_child_curl = curl_easy_init();
    g_child_store = X509_STORE_new();
    children.push_back(spawn_child([] {
      register_store(g_child_curl, g_child_store);
      _exit(0);
    }));
  }
  std::vector<int> statuses;
  for (pid_t pid : children) {
    statuses.push_back(wait_child(pid));
  }
  for (std::thread &registrar : registrars) {
    registrar.join();
  }

  for (int status : statuses) {
    assert_exited_cleanly(status);
  }
}

void test_fork_while_cache_locked(void **unused) {
  SF_UNUSED(unused);
  for (int i = 0; i < 20; ++i) {
    const std::string path =
        g_cache_dir + "/http___crl.example.com_fork_" + std::to_string(i);
    FILE *f = fopen(path.c_str(), "w");
    assert_non_null(f);
    fputs(kFreshCrlPem, f);
    fclose(f);
  }

  // Mutexes are not fair: without a pause between passes the cleaner
  // re-acquires the lock immediately and starves fork(), which must take it.
  std::atomic<bool> stop(false);
  std::thread cleaner([&stop] {
    while (!stop.load()) {
      cleanupCertCRLCache();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  std::vector<pid_t> children;
  for (int i = 0; i < 50; ++i) {
    children.push_back(spawn_child([] {
      cleanupCertCRLCache();
      _exit(0);
    }));
  }
  std::vector<int> statuses;
  for (pid_t pid : children) {
    statuses.push_back(wait_child(pid));
  }

  stop.store(true);
  cleaner.join();

  for (int status : statuses) {
    assert_exited_cleanly(status);
  }
}

int main() {
  g_main_thread = pthread_self();
  g_main_thread_set.store(true);

  char dir[] = "/tmp/crl_fork_XXXXXX";
  if (!mkdtemp(dir)) {
    perror("mkdtemp");
    return 1;
  }
  g_cache_dir = dir;
  setenv("SF_CRL_RESPONSE_CACHE_DIR", dir, 1);
  setenv("SF_CRL_CACHE_CLEANUP_INTERVAL", "3600", 1);
  alarm(120);

  curl_global_init(CURL_GLOBAL_ALL);
  // Order matters: the first test needs a process without a cleanup thread,
  // and the second must be the one that starts it.
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_fork_before_crl_use),
      cmocka_unit_test(test_cleanup_thread_does_not_read_environment),
      cmocka_unit_test(test_fork_after_crl_use),
      cmocka_unit_test(test_disabled_cleanup_not_retried),
      cmocka_unit_test(test_cleanup_thread_uses_latest_settings),
      cmocka_unit_test(test_cleanup_thread_blocks_signals),
      cmocka_unit_test(test_fork_while_cache_locked),
      cmocka_unit_test(test_fork_while_registering),
  };
  int ret = cmocka_run_group_tests(tests, nullptr, nullptr);
  curl_global_cleanup();
  return ret;
}
