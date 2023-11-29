/*
 * Copyright (c) 2023 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>
#include "./lib/ClientQueryContextCache.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

using namespace Snowflake::Client;

static const int MAX_CAPACITY = 5;
static const uint64 BASE_READ_TIMESTAMP = 1668727958;
static const std::string SOME_CONTEXT = "Some query context";
static const uint64 BASE_ID = 0;
static const uint64 BASE_PRIORITY = 0;

// test helper so make everything public
class QueryContextCacheTestHelper
{
public:
  Snowflake::Client::ClientQueryContextCache qcc;
  std::vector<uint64> expectedIDs;
  std::vector<uint64> expectedReadTimestamp;
  std::vector<uint64> expectedPriority;

  QueryContextCacheTestHelper() : qcc(MAX_CAPACITY) {}

  void initCache()
  {
    qcc.clearCache();
    qcc.setCapacity(MAX_CAPACITY);
  }

  void initCacheWithDataWithContext(const std::string& context)
  {
    initCache();
    expectedIDs.resize(MAX_CAPACITY);
    expectedReadTimestamp.resize(MAX_CAPACITY);
    expectedPriority.resize(MAX_CAPACITY);
    for (int i = 0; i < MAX_CAPACITY; i++) {
      expectedIDs[i] = BASE_ID + i;
      expectedReadTimestamp[i] = BASE_READ_TIMESTAMP + i;
      expectedPriority[i] = BASE_PRIORITY + i;
      qcc.merge(expectedIDs[i], expectedReadTimestamp[i], expectedPriority[i], context);
    }
    qcc.syncPriorityMap();
  }

  void initCacheWithData()
  {
    initCacheWithDataWithContext(SOME_CONTEXT);
  }

  void initCacheWithDataInRandomOrder()
  {
    initCache();
    expectedIDs.resize(MAX_CAPACITY);
    expectedReadTimestamp.resize(MAX_CAPACITY);
    expectedPriority.resize(MAX_CAPACITY);
    for (int i = 0; i < MAX_CAPACITY; i++) {
      expectedIDs[i] = BASE_ID + i;
      expectedReadTimestamp[i] = BASE_READ_TIMESTAMP + i;
      expectedPriority[i] = BASE_PRIORITY + i;
    }

    qcc.merge(expectedIDs[3], expectedReadTimestamp[3], expectedPriority[3], SOME_CONTEXT);
    qcc.merge(expectedIDs[2], expectedReadTimestamp[2], expectedPriority[2], SOME_CONTEXT);
    qcc.merge(expectedIDs[4], expectedReadTimestamp[4], expectedPriority[4], SOME_CONTEXT);
    qcc.merge(expectedIDs[0], expectedReadTimestamp[0], expectedPriority[0], SOME_CONTEXT);
    qcc.merge(expectedIDs[1], expectedReadTimestamp[1], expectedPriority[1], SOME_CONTEXT);
    qcc.syncPriorityMap();
  }

  void assertCacheDataWithContext(const std::string& context)
  {
    std::vector<uint64> ids;
    std::vector<uint64> readTimestamps;
    std::vector<uint64> priorities;
    std::vector<std::string> contexts;

    // Compare elements
    size_t size = qcc.getElements(ids, readTimestamps, priorities, contexts);
    assert_int_equal(size, MAX_CAPACITY);
    for (size_t i = 0; i < size; i++) {
      assert_int_equal(expectedIDs[i], ids[i]);
      assert_int_equal(expectedReadTimestamp[i], readTimestamps[i]);
      assert_int_equal(expectedPriority[i], priorities[i]);
      assert_string_equal(context.c_str(), contexts[i].c_str());
    }
  }

  void assertCacheData()
  {
    assertCacheDataWithContext(SOME_CONTEXT);
  }
};

void test_is_empty(void ** unused)
{
  QueryContextCacheTestHelper helper;
  helper.initCache();
  assert_int_equal(helper.qcc.getSize(), 0);
}

void test_with_some_data(void ** unused)
{
  QueryContextCacheTestHelper helper;
  helper.initCacheWithData();
  // Compare elements
  helper.assertCacheData();
}

void test_with_some_data_in_random_order(void ** unused)
{
  QueryContextCacheTestHelper helper;
  helper.initCacheWithDataInRandomOrder();
  // Compare elements
  helper.assertCacheData();
}

void test_more_than_capacity(void ** unused)
{
  QueryContextCacheTestHelper helper;
  helper.initCacheWithData();

  // Add one more element at the end
  int i = MAX_CAPACITY;
  helper.qcc.merge(BASE_ID + i, BASE_READ_TIMESTAMP + i, BASE_PRIORITY + i, SOME_CONTEXT);
  helper.qcc.syncPriorityMap();
  helper.qcc.checkCacheCapacity();

  // Compare elements
  helper.assertCacheData();
}

void test_update_timestamp(void ** unused)
{
  QueryContextCacheTestHelper helper;
  helper.initCacheWithData();

  // Add one more element with new TS with existing id
  int updatedID = 1;
  helper.expectedReadTimestamp[updatedID] = BASE_READ_TIMESTAMP + updatedID + 10;
  helper.qcc.merge(BASE_ID + updatedID,
                   helper.expectedReadTimestamp[updatedID],
                   BASE_PRIORITY + updatedID,
                   SOME_CONTEXT);
  helper.qcc.syncPriorityMap();
  helper.qcc.checkCacheCapacity();

  // Compare elements
  helper.assertCacheData();
}

void test_update_priority(void ** unused)
{
  QueryContextCacheTestHelper helper;
  helper.initCacheWithData();

  // Add one more element with new priority with existing id
  int updatedID = 3;
  uint64 updatedPriority = BASE_PRIORITY + updatedID + 7;

  helper.expectedPriority[updatedID] = updatedPriority;
  helper.qcc.merge(BASE_ID + updatedID,
                   BASE_READ_TIMESTAMP + updatedID,
                   helper.expectedPriority[updatedID],
                   SOME_CONTEXT);
  helper.qcc.syncPriorityMap();
  helper.qcc.checkCacheCapacity();

  for (int i = updatedID; i < MAX_CAPACITY - 1; i++) {
    helper.expectedIDs[i] = helper.expectedIDs[i + 1];
    helper.expectedReadTimestamp[i] = helper.expectedReadTimestamp[i + 1];
    helper.expectedPriority[i] = helper.expectedPriority[i + 1];
  }

  helper.expectedIDs[MAX_CAPACITY - 1] = BASE_ID + updatedID;
  helper.expectedReadTimestamp[MAX_CAPACITY - 1] = BASE_READ_TIMESTAMP + updatedID;
  helper.expectedPriority[MAX_CAPACITY - 1] = updatedPriority;

  // Compare elements
  helper.assertCacheData();
}

void test_add_same_priority(void ** unused)
{
  QueryContextCacheTestHelper helper;
  helper.initCacheWithData();

  // Add one more element with same priority
  int i = MAX_CAPACITY;
  uint64 UpdatedPriority = BASE_PRIORITY + 1;
  helper.qcc.merge(BASE_ID + i, BASE_READ_TIMESTAMP + i, UpdatedPriority, SOME_CONTEXT);
  helper.qcc.syncPriorityMap();
  helper.qcc.checkCacheCapacity();
  helper.expectedIDs[1] = BASE_ID + i;
  helper.expectedReadTimestamp[1] = BASE_READ_TIMESTAMP + i;

  // Compare elements
  helper.assertCacheData();
}

void test_add_same_id_but_stale_timestamp(void ** unused)
{
  QueryContextCacheTestHelper helper;
  helper.initCacheWithData();

  // Add one more element with same id
  int i = 2;
  helper.qcc.merge(BASE_ID + i, BASE_READ_TIMESTAMP + i - 10, BASE_PRIORITY + i, SOME_CONTEXT);
  helper.qcc.syncPriorityMap();
  helper.qcc.checkCacheCapacity();

  // Compare elements
  helper.assertCacheData();
}

void test_empty_cache_with_null_data(void ** unused)
{
  QueryContextCacheTestHelper helper;
  helper.initCacheWithData();

  cJSON * nullData = NULL;
  helper.qcc.deserializeQueryContext(nullData);

  assert_int_equal(helper.qcc.getSize(), 0);
}

void test_empty_cache_with_empty_response_data(void ** unused)
{
  QueryContextCacheTestHelper helper;
  helper.initCacheWithData();

  cJSON * emptyData = snowflake_cJSON_Parse("");
  helper.qcc.deserializeQueryContext(emptyData);

  assert_int_equal(helper.qcc.getSize(), 0);
}

void test_serialize_request_and_deserialize_response_data(void ** unused)
{
  QueryContextCacheTestHelper helper;
  helper.initCacheWithData();
  helper.assertCacheData();

  cJSON* cacheData = helper.qcc.serializeQueryContext();

  // Clear qcc
  helper.qcc.clearCache();
  assert_int_equal(helper.qcc.getSize(), 0);

  helper.qcc.deserializeQueryContextReq(cacheData);

  // Compare elements
  helper.assertCacheData();
}

void test_serialize_request_and_deserialize_response_data_with_empty_context(void ** unused)
{
  QueryContextCacheTestHelper helper;
  // Init qcc
  helper.initCacheWithDataWithContext("");
  helper.assertCacheDataWithContext("");

  cJSON * cacheData = helper.qcc.serializeQueryContext();

  // Clear qcc
  helper.qcc.clearCache();
  assert_int_equal(helper.qcc.getSize(), 0);

  helper.qcc.deserializeQueryContextReq(cacheData);
  helper.assertCacheDataWithContext("");
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  return 0;
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_is_empty),
    cmocka_unit_test(test_with_some_data),
    cmocka_unit_test(test_with_some_data_in_random_order),
    cmocka_unit_test(test_more_than_capacity),
    cmocka_unit_test(test_update_timestamp),
    cmocka_unit_test(test_update_priority),
    cmocka_unit_test(test_add_same_priority),
    cmocka_unit_test(test_add_same_id_but_stale_timestamp),
    cmocka_unit_test(test_empty_cache_with_null_data),
    cmocka_unit_test(test_empty_cache_with_empty_response_data),
    cmocka_unit_test(test_serialize_request_and_deserialize_response_data),
    cmocka_unit_test(test_serialize_request_and_deserialize_response_data_with_empty_context),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, NULL);
  return ret;
}
