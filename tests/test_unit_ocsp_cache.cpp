#include <thread>
#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <vector>
#include "utils/test_setup.h"

/* Test hook from curl vtls (exported by sf_ocsp.c) */
extern "C" {
  extern int sf_ocsp_write_file(const char* file, const char* content);
}

std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void cacheWritingThread(const std::string* filepath, const std::string* content)
{
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait(lock, [] { return ready; });
  lock.unlock();
  int ret = sf_ocsp_write_file(filepath->c_str(), content->c_str());
  int a = 0;
}

void test_concurrent_cache_writing(void **unused)
{
  SF_UNUSED(unused);
  const int num_threads = 10;
  // in the test data writing to file repeat with a 10 digits random number
  const int dataseed_min = 1000000000;
  const int dataseed_max = 9999999999;
  const int datasize = 256000;
  const std::string cacheFile("ocsp_concurrent_test.tmp");
  std::vector<std::thread> threads;
  std::vector<std::string> datastrings;

  std::srand(std::time(nullptr));

  for (int i = 0; i < num_threads; ++i)
  {
    std::string ramdom_str = std::to_string(std::rand() % (dataseed_max - dataseed_min + 1) + dataseed_min);
	std::string data("");
    size_t datalen = 0;
    while (datalen < datasize)
    {
      data += ramdom_str;
      datalen += ramdom_str.size();
    }
    datastrings.push_back(data);
    threads.push_back(std::thread(cacheWritingThread, &cacheFile, &datastrings[i]));
  }

  {
    std::lock_guard<std::mutex> lock(mtx);
    ready = true;
  }
  cv.notify_all();

  for (auto& th : threads)
  {
    th.join();
  }

  std::ifstream file(cacheFile);
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  bool found = false;
  for (auto& data : datastrings)
  {
    if (data == content)
    {
      found = true;
      break;
    }
  }
  assert_true(found);
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_concurrent_cache_writing),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
