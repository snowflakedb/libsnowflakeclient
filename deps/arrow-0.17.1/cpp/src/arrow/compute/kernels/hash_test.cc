// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <locale>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "arrow/array.h"
#include "arrow/buffer.h"
#include "arrow/memory_pool.h"
#include "arrow/status.h"
#include "arrow/table.h"
#include "arrow/testing/gtest_common.h"
#include "arrow/testing/util.h"
#include "arrow/type.h"
#include "arrow/type_traits.h"
#include "arrow/util/checked_cast.h"
#include "arrow/util/decimal.h"

#include "arrow/compute/context.h"
#include "arrow/compute/kernel.h"
#include "arrow/compute/kernels/hash.h"
#include "arrow/compute/kernels/util_internal.h"
#include "arrow/compute/test_util.h"

#include "arrow/ipc/json_simple.h"

namespace arrow {

using internal::checked_cast;

namespace compute {

using StringTypes =
    ::testing::Types<StringType, LargeStringType, BinaryType, LargeBinaryType>;

// ----------------------------------------------------------------------
// Dictionary tests

void CheckUnique(FunctionContext* ctx, const std::shared_ptr<Array>& input,
                 const std::shared_ptr<Array>& expected) {
  std::shared_ptr<Array> result;
  ASSERT_OK(Unique(ctx, input, &result));
  ASSERT_OK(result->ValidateFull());
  // TODO: We probably shouldn't rely on array ordering.
  ASSERT_ARRAYS_EQUAL(*expected, *result);
}

template <typename Type, typename T>
void CheckUnique(FunctionContext* ctx, const std::shared_ptr<DataType>& type,
                 const std::vector<T>& in_values, const std::vector<bool>& in_is_valid,
                 const std::vector<T>& out_values,
                 const std::vector<bool>& out_is_valid) {
  std::shared_ptr<Array> input = _MakeArray<Type, T>(type, in_values, in_is_valid);
  std::shared_ptr<Array> expected = _MakeArray<Type, T>(type, out_values, out_is_valid);

  CheckUnique(ctx, input, expected);
}

// Check that ValueCounts() accepts a 0-length array with null buffers
void CheckValueCountsNull(FunctionContext* ctx, const std::shared_ptr<DataType>& type) {
  std::vector<std::shared_ptr<Buffer>> data_buffers(2);
  Datum input;
  input.value =
      ArrayData::Make(type, 0 /* length */, std::move(data_buffers), 0 /* null_count */);

  std::shared_ptr<Array> ex_values = ArrayFromJSON(type, "[]");
  std::shared_ptr<Array> ex_counts = ArrayFromJSON(int64(), "[]");

  std::shared_ptr<Array> result;
  ASSERT_OK(ValueCounts(ctx, input, &result));
  ASSERT_OK(result->ValidateFull());
  auto result_struct = std::dynamic_pointer_cast<StructArray>(result);
  ASSERT_NE(result_struct->GetFieldByName(kValuesFieldName), nullptr);
  // TODO: We probably shouldn't rely on value ordering.
  ASSERT_ARRAYS_EQUAL(*ex_values, *result_struct->GetFieldByName(kValuesFieldName));
  ASSERT_ARRAYS_EQUAL(*ex_counts, *result_struct->GetFieldByName(kCountsFieldName));
}

void CheckValueCounts(FunctionContext* ctx, const std::shared_ptr<Array>& input,
                      const std::shared_ptr<Array>& expected_values,
                      const std::shared_ptr<Array>& expected_counts) {
  std::shared_ptr<Array> result;
  ASSERT_OK(ValueCounts(ctx, input, &result));
  ASSERT_OK(result->ValidateFull());
  auto result_struct = std::dynamic_pointer_cast<StructArray>(result);
  ASSERT_EQ(result_struct->num_fields(), 2);
  // TODO: We probably shouldn't rely on value ordering.
  ASSERT_ARRAYS_EQUAL(*expected_values, *result_struct->field(kValuesFieldIndex));
  ASSERT_ARRAYS_EQUAL(*expected_counts, *result_struct->field(kCountsFieldIndex));
}

template <typename Type, typename T>
void CheckValueCounts(FunctionContext* ctx, const std::shared_ptr<DataType>& type,
                      const std::vector<T>& in_values,
                      const std::vector<bool>& in_is_valid,
                      const std::vector<T>& out_values,
                      const std::vector<bool>& out_is_valid,
                      const std::vector<int64_t>& out_counts) {
  std::vector<bool> all_valids(out_is_valid.size(), true);
  std::shared_ptr<Array> input = _MakeArray<Type, T>(type, in_values, in_is_valid);
  std::shared_ptr<Array> ex_values = _MakeArray<Type, T>(type, out_values, out_is_valid);
  std::shared_ptr<Array> ex_counts =
      _MakeArray<Int64Type, int64_t>(int64(), out_counts, all_valids);

  CheckValueCounts(ctx, input, ex_values, ex_counts);
}

void CheckDictEncode(FunctionContext* ctx, const std::shared_ptr<Array>& input,
                     const std::shared_ptr<Array>& expected_values,
                     const std::shared_ptr<Array>& expected_indices) {
  auto type = dictionary(expected_indices->type(), expected_values->type());
  DictionaryArray expected(type, expected_indices, expected_values);

  Datum datum_out;
  ASSERT_OK(DictionaryEncode(ctx, input, &datum_out));
  std::shared_ptr<Array> result = MakeArray(datum_out.array());
  ASSERT_OK(result->ValidateFull());

  ASSERT_ARRAYS_EQUAL(expected, *result);
}

template <typename Type, typename T>
void CheckDictEncode(FunctionContext* ctx, const std::shared_ptr<DataType>& type,
                     const std::vector<T>& in_values,
                     const std::vector<bool>& in_is_valid,
                     const std::vector<T>& out_values,
                     const std::vector<bool>& out_is_valid,
                     const std::vector<int32_t>& out_indices) {
  std::shared_ptr<Array> input = _MakeArray<Type, T>(type, in_values, in_is_valid);
  std::shared_ptr<Array> ex_dict = _MakeArray<Type, T>(type, out_values, out_is_valid);
  std::shared_ptr<Array> ex_indices =
      _MakeArray<Int32Type, int32_t>(int32(), out_indices, in_is_valid);
  return CheckDictEncode(ctx, input, ex_dict, ex_indices);
}

class TestHashKernel : public ComputeFixture, public TestBase {};

template <typename Type>
class TestHashKernelPrimitive : public ComputeFixture, public TestBase {};

typedef ::testing::Types<Int8Type, UInt8Type, Int16Type, UInt16Type, Int32Type,
                         UInt32Type, Int64Type, UInt64Type, FloatType, DoubleType,
                         Date32Type, Date64Type>
    PrimitiveDictionaries;

TYPED_TEST_SUITE(TestHashKernelPrimitive, PrimitiveDictionaries);

TYPED_TEST(TestHashKernelPrimitive, Unique) {
  using T = typename TypeParam::c_type;
  auto type = TypeTraits<TypeParam>::type_singleton();
  CheckUnique<TypeParam, T>(&this->ctx_, type, {2, 1, 2, 1}, {true, false, true, true},
                            {2, 0, 1}, {1, 0, 1});
  CheckUnique<TypeParam, T>(&this->ctx_, type, {2, 1, 3, 1}, {false, false, true, true},
                            {0, 3, 1}, {0, 1, 1});

  // Sliced
  CheckUnique(&this->ctx_, ArrayFromJSON(type, "[1, 2, null, 3, 2, null]")->Slice(1, 4),
              ArrayFromJSON(type, "[2, null, 3]"));
}

TYPED_TEST(TestHashKernelPrimitive, ValueCounts) {
  using T = typename TypeParam::c_type;
  auto type = TypeTraits<TypeParam>::type_singleton();
  CheckValueCounts<TypeParam, T>(&this->ctx_, type, {2, 1, 2, 1, 2, 3, 4},
                                 {true, false, true, true, true, true, false},
                                 {2, 0, 1, 3}, {1, 0, 1, 1}, {3, 2, 1, 1});
  CheckValueCounts<TypeParam, T>(&this->ctx_, type, {}, {}, {}, {}, {});
  CheckValueCountsNull(&this->ctx_, type);

  // Sliced
  CheckValueCounts(
      &this->ctx_, ArrayFromJSON(type, "[1, 2, null, 3, 2, null]")->Slice(1, 4),
      ArrayFromJSON(type, "[2, null, 3]"), ArrayFromJSON(int64(), "[2, 1, 1]"));
}

TYPED_TEST(TestHashKernelPrimitive, DictEncode) {
  using T = typename TypeParam::c_type;
  auto type = TypeTraits<TypeParam>::type_singleton();
  CheckDictEncode<TypeParam, T>(&this->ctx_, type, {2, 1, 2, 1, 2, 3},
                                {true, false, true, true, true, true}, {2, 1, 3},
                                {1, 1, 1}, {0, 0, 0, 1, 0, 2});

  // Sliced
  CheckDictEncode(
      &this->ctx_, ArrayFromJSON(type, "[2, 1, null, 4, 3, 1, 42]")->Slice(1, 5),
      ArrayFromJSON(type, "[1, 4, 3]"), ArrayFromJSON(int32(), "[0, null, 1, 2, 0]"));
}

TYPED_TEST(TestHashKernelPrimitive, ZeroChunks) {
  auto type = TypeTraits<TypeParam>::type_singleton();

  Datum result;
  auto zero_chunks = std::make_shared<ChunkedArray>(ArrayVector{}, type);
  ASSERT_OK(DictionaryEncode(&this->ctx_, zero_chunks, &result));

  ASSERT_EQ(result.kind(), Datum::CHUNKED_ARRAY);
  AssertChunkedEqual(*result.chunked_array(),
                     ChunkedArray({}, dictionary(int32(), type)));
}

TYPED_TEST(TestHashKernelPrimitive, PrimitiveResizeTable) {
  using T = typename TypeParam::c_type;

  const int64_t kTotalValues = std::min<int64_t>(INT16_MAX, 1UL << sizeof(T) / 2);
  const int64_t kRepeats = 5;

  std::vector<T> values;
  std::vector<T> uniques;
  std::vector<int32_t> indices;
  std::vector<int64_t> counts;
  for (int64_t i = 0; i < kTotalValues * kRepeats; i++) {
    const auto val = static_cast<T>(i % kTotalValues);
    values.push_back(val);

    if (i < kTotalValues) {
      uniques.push_back(val);
      counts.push_back(kRepeats);
    }
    indices.push_back(static_cast<int32_t>(i % kTotalValues));
  }

  auto type = TypeTraits<TypeParam>::type_singleton();
  CheckUnique<TypeParam, T>(&this->ctx_, type, values, {}, uniques, {});
  CheckValueCounts<TypeParam, T>(&this->ctx_, type, values, {}, uniques, {}, counts);
  CheckDictEncode<TypeParam, T>(&this->ctx_, type, values, {}, uniques, {}, indices);
}

TEST_F(TestHashKernel, UniqueTimeTimestamp) {
  CheckUnique<Time32Type, int32_t>(&this->ctx_, time32(TimeUnit::SECOND), {2, 1, 2, 1},
                                   {true, false, true, true}, {2, 0, 1}, {1, 0, 1});

  CheckUnique<Time64Type, int64_t>(&this->ctx_, time64(TimeUnit::NANO), {2, 1, 2, 1},
                                   {true, false, true, true}, {2, 0, 1}, {1, 0, 1});

  CheckUnique<TimestampType, int64_t>(&this->ctx_, timestamp(TimeUnit::NANO),
                                      {2, 1, 2, 1}, {true, false, true, true}, {2, 0, 1},
                                      {1, 0, 1});
}

TEST_F(TestHashKernel, ValueCountsTimeTimestamp) {
  CheckValueCounts<Time32Type, int32_t>(&this->ctx_, time32(TimeUnit::SECOND),
                                        {2, 1, 2, 1}, {true, false, true, true},
                                        {2, 0, 1}, {1, 0, 1}, {2, 1, 1});

  CheckValueCounts<Time64Type, int64_t>(&this->ctx_, time64(TimeUnit::NANO), {2, 1, 2, 1},
                                        {true, false, true, true}, {2, 0, 1}, {1, 0, 1},
                                        {2, 1, 1});

  CheckValueCounts<TimestampType, int64_t>(&this->ctx_, timestamp(TimeUnit::NANO),
                                           {2, 1, 2, 1}, {true, false, true, true},
                                           {2, 0, 1}, {1, 0, 1}, {2, 1, 1});
}

TEST_F(TestHashKernel, UniqueBoolean) {
  CheckUnique<BooleanType, bool>(&this->ctx_, boolean(), {true, true, false, true},
                                 {true, false, true, true}, {true, false, false},
                                 {1, 0, 1});

  CheckUnique<BooleanType, bool>(&this->ctx_, boolean(), {false, true, false, true},
                                 {true, false, true, true}, {false, false, true},
                                 {1, 0, 1});

  // No nulls
  CheckUnique<BooleanType, bool>(&this->ctx_, boolean(), {true, true, false, true}, {},
                                 {true, false}, {});

  CheckUnique<BooleanType, bool>(&this->ctx_, boolean(), {false, true, false, true}, {},
                                 {false, true}, {});

  // Sliced
  CheckUnique(&this->ctx_,
              ArrayFromJSON(boolean(), "[null, true, true, false]")->Slice(1, 2),
              ArrayFromJSON(boolean(), "[true]"));
}

TEST_F(TestHashKernel, ValueCountsBoolean) {
  CheckValueCounts<BooleanType, bool>(&this->ctx_, boolean(), {true, true, false, true},
                                      {true, false, true, true}, {true, false, false},
                                      {1, 0, 1}, {2, 1, 1});

  CheckValueCounts<BooleanType, bool>(&this->ctx_, boolean(), {false, true, false, true},
                                      {true, false, true, true}, {false, false, true},
                                      {1, 0, 1}, {2, 1, 1});

  // No nulls
  CheckValueCounts<BooleanType, bool>(&this->ctx_, boolean(), {true, true, false, true},
                                      {}, {true, false}, {}, {3, 1});

  CheckValueCounts<BooleanType, bool>(&this->ctx_, boolean(), {false, true, false, true},
                                      {}, {false, true}, {}, {2, 2});

  // Sliced
  CheckValueCounts(&this->ctx_,
                   ArrayFromJSON(boolean(), "[true, false, false, null]")->Slice(1, 2),
                   ArrayFromJSON(boolean(), "[false]"), ArrayFromJSON(int64(), "[2]"));
}

TEST_F(TestHashKernel, DictEncodeBoolean) {
  CheckDictEncode<BooleanType, bool>(
      &this->ctx_, boolean(), {true, true, false, true, false},
      {true, false, true, true, true}, {true, false}, {}, {0, 0, 1, 0, 1});

  CheckDictEncode<BooleanType, bool>(
      &this->ctx_, boolean(), {false, true, false, true, false},
      {true, false, true, true, true}, {false, true}, {}, {0, 0, 0, 1, 0});

  // No nulls
  CheckDictEncode<BooleanType, bool>(&this->ctx_, boolean(),
                                     {true, true, false, true, false}, {}, {true, false},
                                     {}, {0, 0, 1, 0, 1});

  CheckDictEncode<BooleanType, bool>(&this->ctx_, boolean(),
                                     {false, true, false, true, false}, {}, {false, true},
                                     {}, {0, 1, 0, 1, 0});

  // Sliced
  CheckDictEncode(
      &this->ctx_,
      ArrayFromJSON(boolean(), "[false, true, null, true, false]")->Slice(1, 3),
      ArrayFromJSON(boolean(), "[true]"), ArrayFromJSON(int32(), "[0, null, 0]"));
}

template <typename ArrowType>
class TestHashKernelBinaryTypes : public TestHashKernel {
 protected:
  std::shared_ptr<DataType> type() { return TypeTraits<ArrowType>::type_singleton(); }

  void CheckDictEncodeP(const std::vector<std::string>& in_values,
                        const std::vector<bool>& in_is_valid,
                        const std::vector<std::string>& out_values,
                        const std::vector<bool>& out_is_valid,
                        const std::vector<int32_t>& out_indices) {
    CheckDictEncode<ArrowType, std::string>(&this->ctx_, type(), in_values, in_is_valid,
                                            out_values, out_is_valid, out_indices);
  }

  void CheckValueCountsP(const std::vector<std::string>& in_values,
                         const std::vector<bool>& in_is_valid,
                         const std::vector<std::string>& out_values,
                         const std::vector<bool>& out_is_valid,
                         const std::vector<int64_t>& out_counts) {
    CheckValueCounts<ArrowType, std::string>(&this->ctx_, type(), in_values, in_is_valid,
                                             out_values, out_is_valid, out_counts);
  }

  void CheckUniqueP(const std::vector<std::string>& in_values,
                    const std::vector<bool>& in_is_valid,
                    const std::vector<std::string>& out_values,
                    const std::vector<bool>& out_is_valid) {
    CheckUnique<ArrowType, std::string>(&this->ctx_, type(), in_values, in_is_valid,
                                        out_values, out_is_valid);
  }
};

TYPED_TEST_SUITE(TestHashKernelBinaryTypes, StringTypes);

TYPED_TEST(TestHashKernelBinaryTypes, ZeroChunks) {
  auto type = this->type();

  Datum result;
  auto zero_chunks = std::make_shared<ChunkedArray>(ArrayVector{}, type);
  ASSERT_OK(DictionaryEncode(&this->ctx_, zero_chunks, &result));

  ASSERT_EQ(result.kind(), Datum::CHUNKED_ARRAY);
  AssertChunkedEqual(*result.chunked_array(),
                     ChunkedArray({}, dictionary(int32(), type)));
}

TYPED_TEST(TestHashKernelBinaryTypes, TwoChunks) {
  auto type = this->type();

  Datum result;
  auto two_chunks = std::make_shared<ChunkedArray>(
      ArrayVector{
          ArrayFromJSON(type, "[\"a\"]"),
          ArrayFromJSON(type, "[\"b\"]"),
      },
      type);
  ASSERT_OK(DictionaryEncode(&this->ctx_, two_chunks, &result));

  auto dict_type = dictionary(int32(), type);
  auto dictionary = ArrayFromJSON(type, R"(["a", "b"])");

  auto chunk_0 = std::make_shared<DictionaryArray>(
      dict_type, ArrayFromJSON(int32(), "[0]"), dictionary);
  auto chunk_1 = std::make_shared<DictionaryArray>(
      dict_type, ArrayFromJSON(int32(), "[1]"), dictionary);

  ASSERT_EQ(result.kind(), Datum::CHUNKED_ARRAY);
  AssertChunkedEqual(*result.chunked_array(),
                     ChunkedArray({chunk_0, chunk_1}, dict_type));
}

TYPED_TEST(TestHashKernelBinaryTypes, Unique) {
  this->CheckUniqueP({"test", "", "test2", "test"}, {true, false, true, true},
                     {"test", "", "test2"}, {1, 0, 1});

  // Sliced
  CheckUnique(
      &this->ctx_,
      ArrayFromJSON(this->type(), R"(["ab", null, "cd", "ef", "cd", "gh"])")->Slice(1, 4),
      ArrayFromJSON(this->type(), R"([null, "cd", "ef"])"));
}

TYPED_TEST(TestHashKernelBinaryTypes, ValueCounts) {
  this->CheckValueCountsP({"test", "", "test2", "test"}, {true, false, true, true},
                          {"test", "", "test2"}, {1, 0, 1}, {2, 1, 1});

  // Sliced
  CheckValueCounts(
      &this->ctx_,
      ArrayFromJSON(this->type(), R"(["ab", null, "cd", "ab", "cd", "ef"])")->Slice(1, 4),
      ArrayFromJSON(this->type(), R"([null, "cd", "ab"])"),
      ArrayFromJSON(int64(), "[1, 2, 1]"));
}

TYPED_TEST(TestHashKernelBinaryTypes, DictEncode) {
  this->CheckDictEncodeP({"test", "", "test2", "test", "baz"},
                         {true, false, true, true, true}, {"test", "test2", "baz"}, {},
                         {0, 0, 1, 0, 2});

  // Sliced
  CheckDictEncode(
      &this->ctx_,
      ArrayFromJSON(this->type(), R"(["ab", null, "cd", "ab", "cd", "ef"])")->Slice(1, 4),
      ArrayFromJSON(this->type(), R"(["cd", "ab"])"),
      ArrayFromJSON(int32(), "[null, 0, 1, 0]"));
}

TYPED_TEST(TestHashKernelBinaryTypes, BinaryResizeTable) {
  const int32_t kTotalValues = 10000;
#if !defined(ARROW_VALGRIND)
  const int32_t kRepeats = 10;
#else
  // Mitigate Valgrind's slowness
  const int32_t kRepeats = 3;
#endif

  std::vector<std::string> values;
  std::vector<std::string> uniques;
  std::vector<int32_t> indices;
  std::vector<int64_t> counts;
  char buf[20] = "test";

  for (int32_t i = 0; i < kTotalValues * kRepeats; i++) {
    int32_t index = i % kTotalValues;

    ASSERT_GE(snprintf(buf + 4, sizeof(buf) - 4, "%d", index), 0);
    values.emplace_back(buf);

    if (i < kTotalValues) {
      uniques.push_back(values.back());
      counts.push_back(kRepeats);
    }
    indices.push_back(index);
  }

  this->CheckUniqueP(values, {}, uniques, {});
  this->CheckValueCountsP(values, {}, uniques, {}, counts);
  this->CheckDictEncodeP(values, {}, uniques, {}, indices);
}

TEST_F(TestHashKernel, UniqueFixedSizeBinary) {
  auto type = fixed_size_binary(3);

  CheckUnique<FixedSizeBinaryType, std::string>(
      &this->ctx_, type, {"aaa", "", "bbb", "aaa"}, {true, false, true, true},
      {"aaa", "", "bbb"}, {1, 0, 1});

  // Sliced
  CheckUnique(
      &this->ctx_,
      ArrayFromJSON(type, R"(["aaa", null, "bbb", "bbb", "ccc", "ddd"])")->Slice(1, 4),
      ArrayFromJSON(type, R"([null, "bbb", "ccc"])"));
}

TEST_F(TestHashKernel, ValueCountsFixedSizeBinary) {
  auto type = fixed_size_binary(3);
  auto input = ArrayFromJSON(type, R"(["aaa", null, "bbb", "bbb", "ccc", null])");

  CheckValueCounts(&this->ctx_, input,
                   ArrayFromJSON(type, R"(["aaa", null, "bbb", "ccc"])"),
                   ArrayFromJSON(int64(), "[1, 2, 2, 1]"));

  // Sliced
  CheckValueCounts(&this->ctx_, input->Slice(1, 4),
                   ArrayFromJSON(type, R"([null, "bbb", "ccc"])"),
                   ArrayFromJSON(int64(), "[1, 2, 1]"));
}

TEST_F(TestHashKernel, DictEncodeFixedSizeBinary) {
  auto type = fixed_size_binary(3);

  CheckDictEncode<FixedSizeBinaryType, std::string>(
      &this->ctx_, type, {"bbb", "", "bbb", "aaa", "ccc"},
      {true, false, true, true, true}, {"bbb", "aaa", "ccc"}, {}, {0, 0, 0, 1, 2});

  // Sliced
  CheckDictEncode(
      &this->ctx_,
      ArrayFromJSON(type, R"(["aaa", null, "bbb", "bbb", "ccc", "ddd"])")->Slice(1, 4),
      ArrayFromJSON(type, R"(["bbb", "ccc"])"),
      ArrayFromJSON(int32(), "[null, 0, 0, 1]"));
}

TEST_F(TestHashKernel, FixedSizeBinaryResizeTable) {
  const int32_t kTotalValues = 10000;
#if !defined(ARROW_VALGRIND)
  const int32_t kRepeats = 10;
#else
  // Mitigate Valgrind's slowness
  const int32_t kRepeats = 3;
#endif

  std::vector<std::string> values;
  std::vector<std::string> uniques;
  std::vector<int32_t> indices;
  char buf[7] = "test..";

  for (int32_t i = 0; i < kTotalValues * kRepeats; i++) {
    int32_t index = i % kTotalValues;

    buf[4] = static_cast<char>(index / 128);
    buf[5] = static_cast<char>(index % 128);
    values.emplace_back(buf, 6);

    if (i < kTotalValues) {
      uniques.push_back(values.back());
    }
    indices.push_back(index);
  }

  auto type = fixed_size_binary(6);
  CheckUnique<FixedSizeBinaryType, std::string>(&this->ctx_, type, values, {}, uniques,
                                                {});
  CheckDictEncode<FixedSizeBinaryType, std::string>(&this->ctx_, type, values, {},
                                                    uniques, {}, indices);
}

TEST_F(TestHashKernel, UniqueDecimal) {
  std::vector<Decimal128> values{12, 12, 11, 12};
  std::vector<Decimal128> expected{12, 0, 11};

  CheckUnique<Decimal128Type, Decimal128>(&this->ctx_, decimal(2, 0), values,
                                          {true, false, true, true}, expected, {1, 0, 1});
}

TEST_F(TestHashKernel, ValueCountsDecimal) {
  std::vector<Decimal128> values{12, 12, 11, 12};
  std::vector<Decimal128> expected{12, 0, 11};

  CheckValueCounts<Decimal128Type, Decimal128>(&this->ctx_, decimal(2, 0), values,
                                               {true, false, true, true}, expected,
                                               {1, 0, 1}, {2, 1, 1});
}

TEST_F(TestHashKernel, DictEncodeDecimal) {
  std::vector<Decimal128> values{12, 12, 11, 12, 13};
  std::vector<Decimal128> expected{12, 11, 13};

  CheckDictEncode<Decimal128Type, Decimal128>(&this->ctx_, decimal(2, 0), values,
                                              {true, false, true, true, true}, expected,
                                              {}, {0, 0, 1, 0, 2});
}

/* TODO(ARROW-4124): Determine if we want to do something that is reproducible with
 * floats.
TEST_F(TestHashKernel, ValueCountsFloat) {

    // No nulls
  CheckValueCounts<FloatType, float>(&this->ctx_, float32(), {1.0f, 0.0f, -0.0f,
std::nan("1"), std::nan("2")  },
                                      {}, {0.0f, 1.0f, std::nan("1")}, {}, {});

  CheckValueCounts<DoubleType, double>(&this->ctx_, float64(), {1.0f, 0.0f, -0.0f,
std::nan("1"), std::nan("2")  },
                                      {}, {0.0f, 1.0f, std::nan("1")}, {}, {});
}
*/

TEST_F(TestHashKernel, ChunkedArrayInvoke) {
  std::vector<std::string> values1 = {"foo", "bar", "foo"};
  std::vector<std::string> values2 = {"bar", "baz", "quuux", "foo"};

  auto type = utf8();
  auto a1 = _MakeArray<StringType, std::string>(type, values1, {});
  auto a2 = _MakeArray<StringType, std::string>(type, values2, {});

  std::vector<std::string> dict_values = {"foo", "bar", "baz", "quuux"};
  auto ex_dict = _MakeArray<StringType, std::string>(type, dict_values, {});

  std::vector<int64_t> counts = {3, 2, 1, 1};
  auto ex_counts = _MakeArray<Int64Type, int64_t>(int64(), counts, {});

  ArrayVector arrays = {a1, a2};
  auto carr = std::make_shared<ChunkedArray>(arrays);

  // Unique
  std::shared_ptr<Array> result;
  ASSERT_OK(Unique(&this->ctx_, carr, &result));
  ASSERT_ARRAYS_EQUAL(*ex_dict, *result);

  // Dictionary encode
  auto dict_type = dictionary(int32(), type);

  auto i1 = _MakeArray<Int32Type, int32_t>(int32(), {0, 1, 0}, {});
  auto i2 = _MakeArray<Int32Type, int32_t>(int32(), {1, 2, 3, 0}, {});

  ArrayVector dict_arrays = {std::make_shared<DictionaryArray>(dict_type, i1, ex_dict),
                             std::make_shared<DictionaryArray>(dict_type, i2, ex_dict)};
  auto dict_carr = std::make_shared<ChunkedArray>(dict_arrays);

  // Unique counts
  std::shared_ptr<Array> counts_array;
  ASSERT_OK(ValueCounts(&this->ctx_, carr, &counts_array));
  auto counts_struct = std::dynamic_pointer_cast<StructArray>(counts_array);
  ASSERT_ARRAYS_EQUAL(*ex_dict, *counts_struct->field(0));
  ASSERT_ARRAYS_EQUAL(*ex_counts, *counts_struct->field(1));

  // Dictionary encode
  Datum encoded_out;
  ASSERT_OK(DictionaryEncode(&this->ctx_, carr, &encoded_out));
  ASSERT_EQ(Datum::CHUNKED_ARRAY, encoded_out.kind());

  AssertChunkedEqual(*dict_carr, *encoded_out.chunked_array());
}

TEST_F(TestHashKernel, ZeroLengthDictionaryEncode) {
  // ARROW-7008
  auto values = ArrayFromJSON(utf8(), "[]");
  Datum datum_result;
  ASSERT_OK(DictionaryEncode(&this->ctx_, values, &datum_result));

  std::shared_ptr<Array> result = datum_result.make_array();
  const auto& dict_result = checked_cast<const DictionaryArray&>(*result);
  ASSERT_OK(dict_result.Validate());
  ASSERT_OK(dict_result.ValidateFull());
}

TEST_F(TestHashKernel, ChunkedArrayZeroChunk) {
  // ARROW-6857
  auto chunked_array = std::make_shared<ChunkedArray>(ArrayVector{}, utf8());

  std::shared_ptr<Array> result_array, expected;
  Datum result_datum;

  ASSERT_OK(Unique(&this->ctx_, chunked_array, &result_array));
  expected = ArrayFromJSON(chunked_array->type(), "[]");
  AssertArraysEqual(*expected, *result_array);

  ASSERT_OK(ValueCounts(&this->ctx_, chunked_array, &result_array));
  expected = ArrayFromJSON(struct_({field(kValuesFieldName, chunked_array->type()),
                                    field(kCountsFieldName, int64())}),
                           "[]");
  AssertArraysEqual(*expected, *result_array);

  ASSERT_OK(DictionaryEncode(&this->ctx_, chunked_array, &result_datum));
  auto dict_type = dictionary(int32(), chunked_array->type());
  ASSERT_EQ(result_datum.kind(), Datum::CHUNKED_ARRAY);

  AssertChunkedEqual(*std::make_shared<ChunkedArray>(ArrayVector{}, dict_type),
                     *result_datum.chunked_array());
}

}  // namespace compute
}  // namespace arrow
