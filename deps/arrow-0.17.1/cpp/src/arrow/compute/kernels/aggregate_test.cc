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
#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "arrow/array.h"
#include "arrow/compute/kernel.h"
#include "arrow/compute/kernels/count.h"
#include "arrow/compute/kernels/mean.h"
#include "arrow/compute/kernels/minmax.h"
#include "arrow/compute/kernels/sum.h"
#include "arrow/compute/kernels/sum_internal.h"
#include "arrow/compute/test_util.h"
#include "arrow/type.h"
#include "arrow/type_traits.h"
#include "arrow/util/checked_cast.h"

#include "arrow/testing/gtest_common.h"
#include "arrow/testing/gtest_util.h"
#include "arrow/testing/random.h"

namespace arrow {

using internal::checked_cast;
using internal::checked_pointer_cast;

namespace compute {

///
/// Sum
///

template <typename ArrowType>
using SumResult =
    std::pair<typename FindAccumulatorType<ArrowType>::Type::c_type, size_t>;

template <typename ArrowType>
static SumResult<ArrowType> NaiveSumPartial(const Array& array) {
  using ArrayType = typename TypeTraits<ArrowType>::ArrayType;
  using ResultType = SumResult<ArrowType>;

  ResultType result;

  auto data = array.data();
  const auto& array_numeric = reinterpret_cast<const ArrayType&>(array);
  const auto values = array_numeric.raw_values();

  if (array.null_count() != 0) {
    internal::BitmapReader reader(array.null_bitmap_data(), array.offset(),
                                  array.length());
    for (int64_t i = 0; i < array.length(); i++) {
      if (reader.IsSet()) {
        result.first += values[i];
        result.second++;
      }

      reader.Next();
    }
  } else {
    for (int64_t i = 0; i < array.length(); i++) {
      result.first += values[i];
      result.second++;
    }
  }

  return result;
}

template <typename ArrowType>
static Datum NaiveSum(const Array& array) {
  using SumType = typename FindAccumulatorType<ArrowType>::Type;
  using SumScalarType = typename TypeTraits<SumType>::ScalarType;

  auto result = NaiveSumPartial<ArrowType>(array);
  bool is_valid = result.second > 0;

  if (!is_valid) return Datum(std::make_shared<SumScalarType>());
  return Datum(std::make_shared<SumScalarType>(result.first));
}

template <typename ArrowType>
void ValidateSum(FunctionContext* ctx, const Array& input, Datum expected) {
  using OutputType = typename FindAccumulatorType<ArrowType>::Type;

  Datum result;
  ASSERT_OK(Sum(ctx, input, &result));
  DatumEqual<OutputType>::EnsureEqual(result, expected);
}

template <typename ArrowType>
void ValidateSum(FunctionContext* ctx, const std::shared_ptr<ChunkedArray>& input,
                 Datum expected) {
  using OutputType = typename FindAccumulatorType<ArrowType>::Type;

  Datum result;
  ASSERT_OK(Sum(ctx, input, &result));
  DatumEqual<OutputType>::EnsureEqual(result, expected);
}

template <typename ArrowType>
void ValidateSum(FunctionContext* ctx, const char* json, Datum expected) {
  auto array = ArrayFromJSON(TypeTraits<ArrowType>::type_singleton(), json);
  ValidateSum<ArrowType>(ctx, *array, expected);
}

template <typename ArrowType>
void ValidateSum(FunctionContext* ctx, const std::vector<std::string>& json,
                 Datum expected) {
  auto array = ChunkedArrayFromJSON(TypeTraits<ArrowType>::type_singleton(), json);
  ValidateSum<ArrowType>(ctx, array, expected);
}

template <typename ArrowType>
void ValidateSum(FunctionContext* ctx, const Array& array) {
  ValidateSum<ArrowType>(ctx, array, NaiveSum<ArrowType>(array));
}

template <typename ArrowType>
class TestNumericSumKernel : public ComputeFixture, public TestBase {};

TYPED_TEST_SUITE(TestNumericSumKernel, NumericArrowTypes);
TYPED_TEST(TestNumericSumKernel, SimpleSum) {
  using SumType = typename FindAccumulatorType<TypeParam>::Type;
  using ScalarType = typename TypeTraits<SumType>::ScalarType;
  using T = typename TypeParam::c_type;

  ValidateSum<TypeParam>(&this->ctx_, "[]", Datum(std::make_shared<ScalarType>()));

  ValidateSum<TypeParam>(&this->ctx_, "[null]", Datum(std::make_shared<ScalarType>()));

  ValidateSum<TypeParam>(&this->ctx_, "[0, 1, 2, 3, 4, 5]",
                         Datum(std::make_shared<ScalarType>(static_cast<T>(5 * 6 / 2))));

  std::vector<std::string> chunks = {"[0, 1, 2, 3, 4, 5]"};
  ValidateSum<TypeParam>(&this->ctx_, chunks,
                         Datum(std::make_shared<ScalarType>(static_cast<T>(5 * 6 / 2))));

  chunks = {"[0, 1, 2]", "[3, 4, 5]"};
  ValidateSum<TypeParam>(&this->ctx_, chunks,
                         Datum(std::make_shared<ScalarType>(static_cast<T>(5 * 6 / 2))));

  chunks = {"[0, 1, 2]", "[]", "[3, 4, 5]"};
  ValidateSum<TypeParam>(&this->ctx_, chunks,
                         Datum(std::make_shared<ScalarType>(static_cast<T>(5 * 6 / 2))));

  chunks = {};
  ValidateSum<TypeParam>(&this->ctx_, chunks,
                         Datum(std::make_shared<ScalarType>()));  // null

  const T expected_result = static_cast<T>(14);
  ValidateSum<TypeParam>(&this->ctx_, "[1, null, 3, null, 3, null, 7]",
                         Datum(std::make_shared<ScalarType>(expected_result)));
}

template <typename ArrowType>
class TestRandomNumericSumKernel : public ComputeFixture, public TestBase {};

TYPED_TEST_SUITE(TestRandomNumericSumKernel, NumericArrowTypes);
TYPED_TEST(TestRandomNumericSumKernel, RandomArraySum) {
  auto rand = random::RandomArrayGenerator(0x5487655);
  for (size_t i = 3; i < 14; i += 2) {
    for (auto null_probability : {0.0, 0.1, 0.5, 1.0}) {
      for (auto length_adjust : {-2, -1, 0, 1, 2}) {
        int64_t length = (1UL << i) + length_adjust;
        auto array = rand.Numeric<TypeParam>(length, 0, 100, null_probability);
        ValidateSum<TypeParam>(&this->ctx_, *array);
      }
    }
  }
}

TYPED_TEST(TestRandomNumericSumKernel, RandomSliceArraySum) {
  auto arithmetic = ArrayFromJSON(TypeTraits<TypeParam>::type_singleton(),
                                  "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]");
  ValidateSum<TypeParam>(&this->ctx_, *arithmetic);
  for (size_t i = 1; i < 15; i++) {
    auto slice = arithmetic->Slice(i, 16);
    ValidateSum<TypeParam>(&this->ctx_, *slice);
  }

  // Trigger ConsumeSparse with different slice offsets.
  auto rand = random::RandomArrayGenerator(0xfa432643);
  const int64_t length = 1U << 5;
  auto array = rand.Numeric<TypeParam>(length, 0, 10, 0.5);
  for (size_t i = 1; i < 16; i++) {
    for (size_t j = 1; j < 16; j++) {
      auto slice = array->Slice(i, length - j);
      ValidateSum<TypeParam>(&this->ctx_, *slice);
    }
  }
}

///
/// Mean
///

template <typename ArrowType>
static Datum NaiveMean(const Array& array) {
  using MeanScalarType = typename TypeTraits<DoubleType>::ScalarType;

  const auto result = NaiveSumPartial<ArrowType>(array);
  const double mean = static_cast<double>(result.first) /
                      static_cast<double>(result.second ? result.second : 1UL);
  const bool is_valid = result.second > 0;

  if (!is_valid) return Datum(std::make_shared<MeanScalarType>());
  return Datum(std::make_shared<MeanScalarType>(mean));
}

template <typename ArrowType>
void ValidateMean(FunctionContext* ctx, const Array& input, Datum expected) {
  using OutputType = typename FindAccumulatorType<DoubleType>::Type;

  Datum result;
  ASSERT_OK(Mean(ctx, input, &result));
  DatumEqual<OutputType>::EnsureEqual(result, expected);
}

template <typename ArrowType>
void ValidateMean(FunctionContext* ctx, const char* json, Datum expected) {
  auto array = ArrayFromJSON(TypeTraits<ArrowType>::type_singleton(), json);
  ValidateMean<ArrowType>(ctx, *array, expected);
}

template <typename ArrowType>
void ValidateMean(FunctionContext* ctx, const Array& array) {
  ValidateMean<ArrowType>(ctx, array, NaiveMean<ArrowType>(array));
}

template <typename ArrowType>
class TestMeanKernelNumeric : public ComputeFixture, public TestBase {};

TYPED_TEST_SUITE(TestMeanKernelNumeric, NumericArrowTypes);
TYPED_TEST(TestMeanKernelNumeric, SimpleMean) {
  using ScalarType = typename TypeTraits<DoubleType>::ScalarType;

  ValidateMean<TypeParam>(&this->ctx_, "[]", Datum(std::make_shared<ScalarType>()));

  ValidateMean<TypeParam>(&this->ctx_, "[null]", Datum(std::make_shared<ScalarType>()));

  ValidateMean<TypeParam>(&this->ctx_, "[1, null, 1]",
                          Datum(std::make_shared<ScalarType>(1.0)));

  ValidateMean<TypeParam>(&this->ctx_, "[1, 2, 3, 4, 5, 6, 7, 8]",
                          Datum(std::make_shared<ScalarType>(4.5)));

  ValidateMean<TypeParam>(&this->ctx_, "[0, 0, 0, 0, 0, 0, 0, 0]",
                          Datum(std::make_shared<ScalarType>(0.0)));

  ValidateMean<TypeParam>(&this->ctx_, "[1, 1, 1, 1, 1, 1, 1, 1]",
                          Datum(std::make_shared<ScalarType>(1.0)));
}

template <typename ArrowType>
class TestRandomNumericMeanKernel : public ComputeFixture, public TestBase {};

TYPED_TEST_SUITE(TestRandomNumericMeanKernel, NumericArrowTypes);
TYPED_TEST(TestRandomNumericMeanKernel, RandomArrayMean) {
  auto rand = random::RandomArrayGenerator(0x8afc055);
  for (size_t i = 3; i < 14; i += 2) {
    for (auto null_probability : {0.0, 0.1, 0.5, 1.0}) {
      for (auto length_adjust : {-2, -1, 0, 1, 2}) {
        int64_t length = (1UL << i) + length_adjust;
        auto array = rand.Numeric<TypeParam>(length, 0, 100, null_probability);
        ValidateMean<TypeParam>(&this->ctx_, *array);
      }
    }
  }
}

///
/// Count
///
//
using CountPair = std::pair<int64_t, int64_t>;

static CountPair NaiveCount(const Array& array) {
  CountPair count;

  count.first = array.length() - array.null_count();
  count.second = array.null_count();

  return count;
}

void ValidateCount(FunctionContext* ctx, const Array& input, CountPair expected) {
  CountOptions all = CountOptions(CountOptions::COUNT_ALL);
  CountOptions nulls = CountOptions(CountOptions::COUNT_NULL);
  Datum result;

  ASSERT_OK(Count(ctx, all, input, &result));
  AssertDatumsEqual(result, Datum(expected.first));

  ASSERT_OK(Count(ctx, nulls, input, &result));
  AssertDatumsEqual(result, Datum(expected.second));
}

template <typename ArrowType>
void ValidateCount(FunctionContext* ctx, const char* json, CountPair expected) {
  auto array = ArrayFromJSON(TypeTraits<ArrowType>::type_singleton(), json);
  ValidateCount(ctx, *array, expected);
}

void ValidateCount(FunctionContext* ctx, const Array& input) {
  ValidateCount(ctx, input, NaiveCount(input));
}

template <typename ArrowType>
class TestCountKernel : public ComputeFixture, public TestBase {};

TYPED_TEST_SUITE(TestCountKernel, NumericArrowTypes);
TYPED_TEST(TestCountKernel, SimpleCount) {
  ValidateCount<TypeParam>(&this->ctx_, "[]", {0, 0});
  ValidateCount<TypeParam>(&this->ctx_, "[null]", {0, 1});
  ValidateCount<TypeParam>(&this->ctx_, "[1, null, 2]", {2, 1});
  ValidateCount<TypeParam>(&this->ctx_, "[null, null, null]", {0, 3});
  ValidateCount<TypeParam>(&this->ctx_, "[1, 2, 3, 4, 5, 6, 7, 8, 9]", {9, 0});
}

template <typename ArrowType>
class TestRandomNumericCountKernel : public ComputeFixture, public TestBase {};

TYPED_TEST_SUITE(TestRandomNumericCountKernel, NumericArrowTypes);
TYPED_TEST(TestRandomNumericCountKernel, RandomArrayCount) {
  auto rand = random::RandomArrayGenerator(0x1205643);
  for (size_t i = 3; i < 14; i++) {
    for (auto null_probability : {0.0, 0.01, 0.1, 0.25, 0.5, 1.0}) {
      for (auto length_adjust : {-2, -1, 0, 1, 2}) {
        int64_t length = (1UL << i) + length_adjust;
        auto array = rand.Numeric<TypeParam>(length, 0, 100, null_probability);
        ValidateCount(&this->ctx_, *array);
      }
    }
  }
}

///
/// Min / Max
///

template <typename ArrowType>
class TestNumericMinMaxKernel : public ComputeFixture, public TestBase {
  using Traits = TypeTraits<ArrowType>;
  using ArrayType = typename Traits::ArrayType;
  using c_type = typename ArrayType::value_type;
  using ScalarType = typename Traits::ScalarType;

 public:
  void AssertMinMaxIs(const Datum& array, c_type expected_min, c_type expected_max,
                      const MinMaxOptions& options) {
    Datum out;
    ASSERT_OK(MinMax(&this->ctx_, options, array, &out));

    ASSERT_TRUE(out.is_collection());

    Datum out_min = out.collection()[0];
    ASSERT_TRUE(out_min.is_scalar());
    ASSERT_EQ(checked_cast<const ScalarType&>(*out_min.scalar()).value, expected_min);

    Datum out_max = out.collection()[1];
    ASSERT_TRUE(out_max.is_scalar());
    ASSERT_EQ(checked_cast<const ScalarType&>(*out_max.scalar()).value, expected_max);
  }

  void AssertMinMaxIs(const std::string& json, c_type expected_min, c_type expected_max,
                      const MinMaxOptions& options) {
    auto array = ArrayFromJSON(Traits::type_singleton(), json);
    AssertMinMaxIs(array, expected_min, expected_max, options);
  }

  void AssertMinMaxIs(const std::vector<std::string>& json, c_type expected_min,
                      c_type expected_max, const MinMaxOptions& options) {
    auto array = ChunkedArrayFromJSON(Traits::type_singleton(), json);
    AssertMinMaxIs(array, expected_min, expected_max, options);
  }

  void AssertMinMaxIsNull(const Datum& array, const MinMaxOptions& options) {
    Datum out;
    ASSERT_OK(MinMax(&this->ctx_, options, array, &out));

    ASSERT_TRUE(out.is_collection());
    for (const auto& item : out.collection()) {
      ASSERT_TRUE(item.is_scalar());
      ASSERT_FALSE(item.scalar()->is_valid);
    }
  }

  void AssertMinMaxIsNull(const std::string& json, const MinMaxOptions& options) {
    auto array = ArrayFromJSON(Traits::type_singleton(), json);
    AssertMinMaxIsNull(array, options);
  }

  void AssertMinMaxIsNull(const std::vector<std::string>& json,
                          const MinMaxOptions& options) {
    auto array = ChunkedArrayFromJSON(Traits::type_singleton(), json);
    AssertMinMaxIsNull(array, options);
  }
};

template <typename ArrowType>
class TestFloatingMinMaxKernel : public TestNumericMinMaxKernel<ArrowType> {};

TYPED_TEST_SUITE(TestNumericMinMaxKernel, IntegralArrowTypes);
TYPED_TEST(TestNumericMinMaxKernel, Basics) {
  MinMaxOptions options;
  std::vector<std::string> chunked_input1 = {"[5, 1, 2, 3, 4]", "[9, 1, null, 3, 4]"};
  std::vector<std::string> chunked_input2 = {"[5, null, 2, 3, 4]", "[9, 1, 2, 3, 4]"};
  std::vector<std::string> chunked_input3 = {"[5, 1, 2, 3, null]", "[9, 1, null, 3, 4]"};

  this->AssertMinMaxIs("[5, 1, 2, 3, 4]", 1, 5, options);
  this->AssertMinMaxIs("[5, null, 2, 3, 4]", 2, 5, options);
  this->AssertMinMaxIs(chunked_input1, 1, 9, options);
  this->AssertMinMaxIs(chunked_input2, 1, 9, options);
  this->AssertMinMaxIs(chunked_input3, 1, 9, options);

  options = MinMaxOptions(MinMaxOptions::OUTPUT_NULL);
  this->AssertMinMaxIs("[5, 1, 2, 3, 4]", 1, 5, options);
  // output null
  this->AssertMinMaxIsNull("[5, null, 2, 3, 4]", options);
  // output null
  this->AssertMinMaxIsNull(chunked_input1, options);
  this->AssertMinMaxIsNull(chunked_input2, options);
  this->AssertMinMaxIsNull(chunked_input3, options);
}

TYPED_TEST_SUITE(TestFloatingMinMaxKernel, RealArrowTypes);
TYPED_TEST(TestFloatingMinMaxKernel, Floats) {
  MinMaxOptions options;
  std::vector<std::string> chunked_input1 = {"[5, 1, 2, 3, 4]", "[9, 1, null, 3, 4]"};
  std::vector<std::string> chunked_input2 = {"[5, null, 2, 3, 4]", "[9, 1, 2, 3, 4]"};
  std::vector<std::string> chunked_input3 = {"[5, 1, 2, 3, null]", "[9, 1, null, 3, 4]"};

  this->AssertMinMaxIs("[5, 1, 2, 3, 4]", 1, 5, options);
  this->AssertMinMaxIs("[5, 1, 2, 3, 4]", 1, 5, options);
  this->AssertMinMaxIs("[5, null, 2, 3, 4]", 2, 5, options);
  this->AssertMinMaxIs("[5, Inf, 2, 3, 4]", 2.0, INFINITY, options);
  this->AssertMinMaxIs("[5, NaN, 2, 3, 4]", 2, 5, options);
  this->AssertMinMaxIs("[5, -Inf, 2, 3, 4]", -INFINITY, 5, options);
  this->AssertMinMaxIs(chunked_input1, 1, 9, options);
  this->AssertMinMaxIs(chunked_input2, 1, 9, options);
  this->AssertMinMaxIs(chunked_input3, 1, 9, options);

  options = MinMaxOptions(MinMaxOptions::OUTPUT_NULL);
  this->AssertMinMaxIs("[5, 1, 2, 3, 4]", 1, 5, options);
  this->AssertMinMaxIs("[5, -Inf, 2, 3, 4]", -INFINITY, 5, options);
  // output null
  this->AssertMinMaxIsNull("[5, null, 2, 3, 4]", options);
  // output null
  this->AssertMinMaxIsNull("[5, -Inf, null, 3, 4]", options);
  // output null
  this->AssertMinMaxIsNull(chunked_input1, options);
  this->AssertMinMaxIsNull(chunked_input2, options);
  this->AssertMinMaxIsNull(chunked_input3, options);
}

}  // namespace compute
}  // namespace arrow
