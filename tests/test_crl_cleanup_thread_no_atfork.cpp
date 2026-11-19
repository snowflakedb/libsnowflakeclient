#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <curl/curl.h>
#include <openssl/x509.h>

#include "utils/test_setup.h"

/*
 * Tests for the CRL cache cleanup thread when its fork handlers are not run:
 * registering them failed, or a fork skipped them.
 */

extern "C" {
struct Curl_easy;
void registerCRLCheck(struct Curl_easy *data, X509_STORE *ctx,
                      bool crl_advisory, bool crl_allow_no_crl,
                      bool crl_disk_caching, bool crl_memory_caching,
                      long crl_download_timeout, long crl_download_max_size);
int isCertCRLCleanupThreadRunning(void);
}

#define ATFORK_RESULT_ENV "SF_TEST_ATFORK_RESULT"

#ifdef __GLIBC__
#define SF_ATFORK_NOEXCEPT noexcept
#else
#define SF_ATFORK_NOEXCEPT
#endif

// Replaces libc pthread_atfork() for the statically linked library code. 0
// drops the handlers, as a fork that skips them. The result comes from the
// environment because other libraries linked into the test may initialize curl,
// and so register the CRL fork handlers, before main().
extern "C" int pthread_atfork(void (*)(void), void (*)(void), void (*)(void)) SF_ATFORK_NOEXCEPT {
  const char *result = getenv(ATFORK_RESULT_ENV);
  return result ? atoi(result) : 0;
}

namespace {

const char *g_self = nullptr;

void register_crl_check() {
  registerCRLCheck(static_cast<struct Curl_easy *>(curl_easy_init()), X509_STORE_new(),
                   false, false, true, true, 10, 0);
}

int wait_child(pid_t pid) {
  int status = 0;
  if (pid <= 0 || waitpid(pid, &status, 0) != pid) {
    return -1;
  }
  return status;
}

pid_t spawn_child(void (*body)()) {
  fflush(stdout);
  fflush(stderr);
  pid_t pid = fork();
  if (pid == 0) {
    signal(SIGSEGV, SIG_DFL);
    signal(SIGALRM, SIG_DFL);
    alarm(10);
    body();
    _exit(99);
  }
  return pid;
}

// Runs a scenario in a new image of this test, so that its pthread_atfork()
// result applies from process start.
int run_scenario(const char *name, int atfork_result) {
  fflush(stdout);
  fflush(stderr);
  pid_t pid = fork();
  if (pid == 0) {
    char result[16];
    snprintf(result, sizeof(result), "%d", atfork_result);
    setenv(ATFORK_RESULT_ENV, result, 1);
    execl(g_self, g_self, name, static_cast<char *>(nullptr));
    _exit(98);
  }
  return wait_child(pid);
}

void assert_exited_cleanly(int status) {
  assert_int_not_equal(status, -1);
  if (WIFSIGNALED(status)) {
    fail_msg("child was killed by signal %d", WTERMSIG(status));
  }
  assert_true(WIFEXITED(status));
  assert_int_equal(WEXITSTATUS(status), 0);
}

int scenario_atfork_fails() {
  curl_global_init(CURL_GLOBAL_ALL);
  register_crl_check();
  if (isCertCRLCleanupThreadRunning()) {
    return 2;
  }
  curl_global_cleanup();
  return 0;
}

// The child must neither see nor join the parent's thread.
int scenario_fork_skipping_handlers() {
  curl_global_init(CURL_GLOBAL_ALL);
  register_crl_check();
  if (!isCertCRLCleanupThreadRunning()) {
    return 3;
  }
  // Without the handlers, a fork during the thread's first cleanup pass would
  // leave the child with the cache lock held.
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  const int status = wait_child(spawn_child([] {
    if (isCertCRLCleanupThreadRunning()) {
      _exit(4);
    }
    curl_global_cleanup();
    exit(0);
  }));
  if (!WIFEXITED(status)) {
    return 5;
  }
  return WEXITSTATUS(status);
}

} // namespace

void test_no_cleanup_thread_if_atfork_fails(void **unused) {
  SF_UNUSED(unused);
  assert_exited_cleanly(run_scenario("atfork_fails", ENOMEM));
}

void test_fork_skipping_handlers(void **unused) {
  SF_UNUSED(unused);
  assert_exited_cleanly(run_scenario("fork_skipping_handlers", 0));
}

int main(int argc, char **argv) {
  if (argc > 1) {
    signal(SIGALRM, SIG_DFL);
    alarm(10);
    int result = 97;
    if (strcmp(argv[1], "atfork_fails") == 0) {
      result = scenario_atfork_fails();
    } else if (strcmp(argv[1], "fork_skipping_handlers") == 0) {
      result = scenario_fork_skipping_handlers();
    }
    exit(result);
  }

  g_self = argv[0];
  char dir[] = "/tmp/crl_no_atfork_XXXXXX";
  if (!mkdtemp(dir)) {
    perror("mkdtemp");
    return 1;
  }
  setenv("SF_CRL_RESPONSE_CACHE_DIR", dir, 1);
  setenv("SF_CRL_CACHE_CLEANUP_INTERVAL", "3600", 1);
  alarm(60);

  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_no_cleanup_thread_if_atfork_fails),
      cmocka_unit_test(test_fork_skipping_handlers),
  };
  return cmocka_run_group_tests(tests, nullptr, nullptr);
}
