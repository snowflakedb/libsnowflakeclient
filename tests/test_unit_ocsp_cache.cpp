#include <thread>
#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <boost/filesystem.hpp>
#include "utils/test_setup.h"
#include "cJSON.h"

/* Test hook from curl vtls (exported by sf_ocsp.c) */
extern "C" {
  extern int sf_ocsp_write_cache(const char* cache_dir, const char* content);
}

std::mutex mtx;
std::condition_variable cv;
bool ready = false;
static const std::string cacheFileName("ocsp_response_cache.json");

std::string loadFileInString(const std::string& file)
{
  std::ifstream fs(file);
  std::stringstream buffer;
  buffer << fs.rdbuf();
  return buffer.str();
}

void cacheWritingThread(size_t dataseed)
{
  std::string data("");
  size_t datalen = 0;
  std::string seedStr = std::to_string(dataseed);
  while (datalen < 256000)
  {
    data += seedStr;
    datalen += seedStr.size();
  }
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait(lock, [] { return ready; });
  lock.unlock();
  // write cache file in current folder
  sf_ocsp_write_cache(".", data.c_str());
}

void test_concurrent_cache_writing(void **unused)
{
  SF_UNUSED(unused);
  const int num_threads = 10;
  // in the test data writing to file repeat with a 10 digits random number
  const size_t dataseed_min = 1000000000;
  const size_t dataseed_max = 9999999999;
  const size_t datasize = 256000;
  std::vector<std::thread> threads;
  std::vector<std::string> datastrings;

  std::srand(std::time(nullptr));

  for (int i = 0; i < num_threads; ++i)
  {
    size_t randnum = std::rand() % (dataseed_max - dataseed_min + 1) + dataseed_min;
    std::string ramdom_str = std::to_string(randnum);
	std::string data("");
    size_t datalen = 0;
    while (datalen < datasize)
    {
      data += ramdom_str;
      datalen += ramdom_str.size();
    }
    datastrings.push_back(data);
    threads.push_back(std::thread(cacheWritingThread, randnum));
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

  std::string content = loadFileInString(cacheFileName);

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

void test_corrupted_cache(void **unused)
{
  SF_UNUSED(unused);
  char cache_file[4096];
  cJSON * ocsp_cache_root = NULL;
  SF_CONNECT* sf;
  SF_STATUS status;

  // make connection first to ensure ocsp cache is generated.
  sf = setup_snowflake_connection();
  status = snowflake_connect(sf);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  snowflake_term(sf);
  get_ocsp_cache_file(cache_file);
  assert_true(boost::filesystem::is_regular_file(cache_file));

  std::string ocsp_response_cache = loadFileInString(cache_file);
  ocsp_cache_root = snowflake_cJSON_Parse(ocsp_response_cache.c_str());
  assert_non_null(ocsp_cache_root);

  // corrupt (truncate) value in every entry
  cJSON* element_pointer = NULL;
  snowflake_cJSON_ArrayForEach(element_pointer, ocsp_cache_root)
  {
    cJSON * value = snowflake_cJSON_GetArrayItem(element_pointer, 1);
    assert_non_null(value);
    size_t valuelen = strlen(value->valuestring);
    if (valuelen > 0)
    {
      value->valuestring[valuelen - 1] = '\0';
    }
  }
  char * jsonText = snowflake_cJSON_PrintUnformatted(ocsp_cache_root);
  std::string cacheDir = std::string(cache_file);
  // remove /ocsp_response_cache.json from the end
  cacheDir = cacheDir.substr(0, cacheDir.size() - cacheFileName.size() - 1);
  sf_ocsp_write_cache(cacheDir.c_str(), jsonText);
  snowflake_cJSON_free(jsonText);

  // connection should success with corrupted cache
  sf = setup_snowflake_connection();
  status = snowflake_connect(sf);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  snowflake_term(sf);
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_concurrent_cache_writing),
    cmocka_unit_test(test_corrupted_cache),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
