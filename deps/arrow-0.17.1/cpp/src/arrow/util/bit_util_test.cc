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
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "arrow/buffer.h"
#include "arrow/memory_pool.h"
#include "arrow/testing/gtest_common.h"
#include "arrow/testing/gtest_util.h"
#include "arrow/util/bit_stream_utils.h"
#include "arrow/util/bit_util.h"
#include "arrow/util/cpu_info.h"

namespace arrow {

using internal::BitmapAnd;
using internal::BitmapOr;
using internal::BitmapXor;
using internal::BitsetStack;
using internal::CopyBitmap;
using internal::CountSetBits;
using internal::InvertBitmap;

template <class BitmapWriter>
void WriteVectorToWriter(BitmapWriter& writer, const std::vector<int> values) {
  for (const auto& value : values) {
    if (value) {
      writer.Set();
    } else {
      writer.Clear();
    }
    writer.Next();
  }
  writer.Finish();
}

void BitmapFromVector(const std::vector<int>& values, int64_t bit_offset,
                      std::shared_ptr<Buffer>* out_buffer, int64_t* out_length) {
  const int64_t length = values.size();
  *out_length = length;
  ASSERT_OK_AND_ASSIGN(*out_buffer, AllocateEmptyBitmap(length + bit_offset));
  auto writer = internal::BitmapWriter((*out_buffer)->mutable_data(), bit_offset, length);
  WriteVectorToWriter(writer, values);
}

#define ASSERT_READER_SET(reader)    \
  do {                               \
    ASSERT_TRUE(reader.IsSet());     \
    ASSERT_FALSE(reader.IsNotSet()); \
    reader.Next();                   \
  } while (false)

#define ASSERT_READER_NOT_SET(reader) \
  do {                                \
    ASSERT_FALSE(reader.IsSet());     \
    ASSERT_TRUE(reader.IsNotSet());   \
    reader.Next();                    \
  } while (false)

// Assert that a BitmapReader yields the given bit values
void ASSERT_READER_VALUES(internal::BitmapReader& reader, std::vector<int> values) {
  for (const auto& value : values) {
    if (value) {
      ASSERT_READER_SET(reader);
    } else {
      ASSERT_READER_NOT_SET(reader);
    }
  }
}

// Assert equal contents of a memory area and a vector of bytes
void ASSERT_BYTES_EQ(const uint8_t* left, const std::vector<uint8_t>& right) {
  auto left_array = std::vector<uint8_t>(left, left + right.size());
  ASSERT_EQ(left_array, right);
}

TEST(BitUtilTests, TestIsMultipleOf64) {
  using BitUtil::IsMultipleOf64;
  EXPECT_TRUE(IsMultipleOf64(64));
  EXPECT_TRUE(IsMultipleOf64(0));
  EXPECT_TRUE(IsMultipleOf64(128));
  EXPECT_TRUE(IsMultipleOf64(192));
  EXPECT_FALSE(IsMultipleOf64(23));
  EXPECT_FALSE(IsMultipleOf64(32));
}

TEST(BitUtilTests, TestNextPower2) {
  using BitUtil::NextPower2;

  ASSERT_EQ(8, NextPower2(6));
  ASSERT_EQ(8, NextPower2(8));

  ASSERT_EQ(1, NextPower2(1));
  ASSERT_EQ(256, NextPower2(131));

  ASSERT_EQ(1024, NextPower2(1000));

  ASSERT_EQ(4096, NextPower2(4000));

  ASSERT_EQ(65536, NextPower2(64000));

  ASSERT_EQ(1LL << 32, NextPower2((1LL << 32) - 1));
  ASSERT_EQ(1LL << 31, NextPower2((1LL << 31) - 1));
  ASSERT_EQ(1LL << 62, NextPower2((1LL << 62) - 1));
}

TEST(BitUtilTests, BytesForBits) {
  using BitUtil::BytesForBits;

  ASSERT_EQ(BytesForBits(0), 0);
  ASSERT_EQ(BytesForBits(1), 1);
  ASSERT_EQ(BytesForBits(7), 1);
  ASSERT_EQ(BytesForBits(8), 1);
  ASSERT_EQ(BytesForBits(9), 2);
  ASSERT_EQ(BytesForBits(0xffff), 8192);
  ASSERT_EQ(BytesForBits(0x10000), 8192);
  ASSERT_EQ(BytesForBits(0x10001), 8193);
  ASSERT_EQ(BytesForBits(0x7ffffffffffffff8ll), 0x0fffffffffffffffll);
  ASSERT_EQ(BytesForBits(0x7ffffffffffffff9ll), 0x1000000000000000ll);
  ASSERT_EQ(BytesForBits(0x7fffffffffffffffll), 0x1000000000000000ll);
}

TEST(BitmapReader, NormalOperation) {
  std::shared_ptr<Buffer> buffer;
  int64_t length;

  for (int64_t offset : {0, 1, 3, 5, 7, 8, 12, 13, 21, 38, 75, 120}) {
    BitmapFromVector({0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1}, offset, &buffer,
                     &length);
    ASSERT_EQ(length, 14);

    auto reader = internal::BitmapReader(buffer->mutable_data(), offset, length);
    ASSERT_READER_VALUES(reader, {0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1});
  }
}

TEST(BitmapReader, DoesNotReadOutOfBounds) {
  uint8_t bitmap[16] = {0};

  const int length = 128;

  internal::BitmapReader r1(bitmap, 0, length);

  // If this were to read out of bounds, valgrind would tell us
  for (int i = 0; i < length; ++i) {
    ASSERT_TRUE(r1.IsNotSet());
    r1.Next();
  }

  internal::BitmapReader r2(bitmap, 5, length - 5);

  for (int i = 0; i < (length - 5); ++i) {
    ASSERT_TRUE(r2.IsNotSet());
    r2.Next();
  }

  // Does not access invalid memory
  internal::BitmapReader r3(nullptr, 0, 0);
}

TEST(BitmapWriter, NormalOperation) {
  for (const auto fill_byte_int : {0x00, 0xff}) {
    const uint8_t fill_byte = static_cast<uint8_t>(fill_byte_int);
    {
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      auto writer = internal::BitmapWriter(bitmap, 0, 12);
      WriteVectorToWriter(writer, {0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1});
      //                      {0b00110110, 0b....1010, ........, ........}
      ASSERT_BYTES_EQ(bitmap, {0x36, static_cast<uint8_t>(0x0a | (fill_byte & 0xf0)),
                               fill_byte, fill_byte});
    }
    {
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      auto writer = internal::BitmapWriter(bitmap, 3, 12);
      WriteVectorToWriter(writer, {0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1});
      //                      {0b10110..., 0b.1010001, ........, ........}
      ASSERT_BYTES_EQ(bitmap, {static_cast<uint8_t>(0xb0 | (fill_byte & 0x07)),
                               static_cast<uint8_t>(0x51 | (fill_byte & 0x80)), fill_byte,
                               fill_byte});
    }
    {
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      auto writer = internal::BitmapWriter(bitmap, 20, 12);
      WriteVectorToWriter(writer, {0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1});
      //                      {........, ........, 0b0110...., 0b10100011}
      ASSERT_BYTES_EQ(bitmap, {fill_byte, fill_byte,
                               static_cast<uint8_t>(0x60 | (fill_byte & 0x0f)), 0xa3});
    }
    // 0-length writes
    for (int64_t pos = 0; pos < 32; ++pos) {
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      auto writer = internal::BitmapWriter(bitmap, pos, 0);
      WriteVectorToWriter(writer, {});
      ASSERT_BYTES_EQ(bitmap, {fill_byte, fill_byte, fill_byte, fill_byte});
    }
  }
}

TEST(BitmapWriter, DoesNotWriteOutOfBounds) {
  uint8_t bitmap[16] = {0};

  const int length = 128;

  int64_t num_values = 0;

  internal::BitmapWriter r1(bitmap, 0, length);

  // If this were to write out of bounds, valgrind would tell us
  for (int i = 0; i < length; ++i) {
    r1.Set();
    r1.Clear();
    r1.Next();
  }
  r1.Finish();
  num_values = r1.position();

  ASSERT_EQ(length, num_values);

  internal::BitmapWriter r2(bitmap, 5, length - 5);

  for (int i = 0; i < (length - 5); ++i) {
    r2.Set();
    r2.Clear();
    r2.Next();
  }
  r2.Finish();
  num_values = r2.position();

  ASSERT_EQ((length - 5), num_values);
}

TEST(FirstTimeBitmapWriter, NormalOperation) {
  for (const auto fill_byte_int : {0x00, 0xff}) {
    const uint8_t fill_byte = static_cast<uint8_t>(fill_byte_int);
    {
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      auto writer = internal::FirstTimeBitmapWriter(bitmap, 0, 12);
      WriteVectorToWriter(writer, {0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1});
      //                      {0b00110110, 0b1010, 0, 0}
      ASSERT_BYTES_EQ(bitmap, {0x36, 0x0a});
    }
    {
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      auto writer = internal::FirstTimeBitmapWriter(bitmap, 4, 12);
      WriteVectorToWriter(writer, {0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1});
      //                      {0b00110110, 0b1010, 0, 0}
      ASSERT_BYTES_EQ(bitmap, {static_cast<uint8_t>(0x60 | (fill_byte & 0x0f)), 0xa3});
    }
    // Consecutive write chunks
    {
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      {
        auto writer = internal::FirstTimeBitmapWriter(bitmap, 0, 6);
        WriteVectorToWriter(writer, {0, 1, 1, 0, 1, 1});
      }
      {
        auto writer = internal::FirstTimeBitmapWriter(bitmap, 6, 3);
        WriteVectorToWriter(writer, {0, 0, 0});
      }
      {
        auto writer = internal::FirstTimeBitmapWriter(bitmap, 9, 3);
        WriteVectorToWriter(writer, {1, 0, 1});
      }
      ASSERT_BYTES_EQ(bitmap, {0x36, 0x0a});
    }
    {
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      {
        auto writer = internal::FirstTimeBitmapWriter(bitmap, 4, 0);
        WriteVectorToWriter(writer, {});
      }
      {
        auto writer = internal::FirstTimeBitmapWriter(bitmap, 4, 6);
        WriteVectorToWriter(writer, {0, 1, 1, 0, 1, 1});
      }
      {
        auto writer = internal::FirstTimeBitmapWriter(bitmap, 10, 3);
        WriteVectorToWriter(writer, {0, 0, 0});
      }
      {
        auto writer = internal::FirstTimeBitmapWriter(bitmap, 13, 0);
        WriteVectorToWriter(writer, {});
      }
      {
        auto writer = internal::FirstTimeBitmapWriter(bitmap, 13, 3);
        WriteVectorToWriter(writer, {1, 0, 1});
      }
      ASSERT_BYTES_EQ(bitmap, {static_cast<uint8_t>(0x60 | (fill_byte & 0x0f)), 0xa3});
    }
  }
}

// Tests for GenerateBits and GenerateBitsUnrolled

struct GenerateBitsFunctor {
  template <class Generator>
  void operator()(uint8_t* bitmap, int64_t start_offset, int64_t length, Generator&& g) {
    return internal::GenerateBits(bitmap, start_offset, length, g);
  }
};

struct GenerateBitsUnrolledFunctor {
  template <class Generator>
  void operator()(uint8_t* bitmap, int64_t start_offset, int64_t length, Generator&& g) {
    return internal::GenerateBitsUnrolled(bitmap, start_offset, length, g);
  }
};

template <typename T>
class TestGenerateBits : public ::testing::Test {};

typedef ::testing::Types<GenerateBitsFunctor, GenerateBitsUnrolledFunctor>
    GenerateBitsTypes;
TYPED_TEST_SUITE(TestGenerateBits, GenerateBitsTypes);

TYPED_TEST(TestGenerateBits, NormalOperation) {
  const int kSourceSize = 256;
  uint8_t source[kSourceSize];
  random_bytes(kSourceSize, 0, source);

  const int64_t start_offsets[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 21, 31, 32};
  const int64_t lengths[] = {0,  1,  2,  3,  4,   5,   6,   7,   8,   9,   12,  16,
                             17, 21, 31, 32, 100, 201, 202, 203, 204, 205, 206, 207};
  const uint8_t fill_bytes[] = {0x00, 0xff};

  for (const int64_t start_offset : start_offsets) {
    for (const int64_t length : lengths) {
      for (const uint8_t fill_byte : fill_bytes) {
        uint8_t bitmap[kSourceSize + 1];
        memset(bitmap, fill_byte, kSourceSize + 1);
        // First call GenerateBits
        {
          int64_t ncalled = 0;
          internal::BitmapReader reader(source, 0, length);
          TypeParam()(bitmap, start_offset, length, [&]() -> bool {
            bool b = reader.IsSet();
            reader.Next();
            ++ncalled;
            return b;
          });
          ASSERT_EQ(ncalled, length);
        }
        // Then check generated contents
        {
          internal::BitmapReader source_reader(source, 0, length);
          internal::BitmapReader result_reader(bitmap, start_offset, length);
          for (int64_t i = 0; i < length; ++i) {
            ASSERT_EQ(source_reader.IsSet(), result_reader.IsSet())
                << "mismatch at bit #" << i;
            source_reader.Next();
            result_reader.Next();
          }
        }
        // Check bits preceding generated contents weren't clobbered
        {
          internal::BitmapReader reader_before(bitmap, 0, start_offset);
          for (int64_t i = 0; i < start_offset; ++i) {
            ASSERT_EQ(reader_before.IsSet(), fill_byte == 0xff)
                << "mismatch at preceding bit #" << start_offset - i;
          }
        }
        // Check the byte following generated contents wasn't clobbered
        auto byte_after = bitmap[BitUtil::CeilDiv(start_offset + length, 8)];
        ASSERT_EQ(byte_after, fill_byte);
      }
    }
  }
}

// Tests for VisitBits and VisitBitsUnrolled. Based on the tests for GenerateBits and
// GenerateBitsUnrolled.
struct VisitBitsFunctor {
  void operator()(const uint8_t* bitmap, int64_t start_offset, int64_t length,
                  bool* destination) {
    auto writer = [&](const bool& bit_value) { *destination++ = bit_value; };
    return internal::VisitBits(bitmap, start_offset, length, writer);
  }
};

struct VisitBitsUnrolledFunctor {
  void operator()(const uint8_t* bitmap, int64_t start_offset, int64_t length,
                  bool* destination) {
    auto writer = [&](const bool& bit_value) { *destination++ = bit_value; };
    return internal::VisitBitsUnrolled(bitmap, start_offset, length, writer);
  }
};

/* Define a typed test class with some utility members. */
template <typename T>
class TestVisitBits : public ::testing::Test {
 protected:
  // The bitmap size that will be used throughout the VisitBits tests.
  static const int64_t kBitmapSizeInBytes = 32;

  // Typedefs for the source and expected destination types in this test.
  using PackedBitmapType = std::array<uint8_t, kBitmapSizeInBytes>;
  using UnpackedBitmapType = std::array<bool, 8 * kBitmapSizeInBytes>;

  // Helper functions to generate the source bitmap and expected destination
  // arrays.
  static PackedBitmapType generate_packed_bitmap() {
    PackedBitmapType bitmap;
    // Assign random values into the source array.
    random_bytes(kBitmapSizeInBytes, 0, bitmap.data());
    return bitmap;
  }

  static UnpackedBitmapType generate_unpacked_bitmap(PackedBitmapType bitmap) {
    // Use a BitmapReader (tested earlier) to populate the expected
    // unpacked bitmap.
    UnpackedBitmapType result;
    internal::BitmapReader reader(bitmap.data(), 0, 8 * kBitmapSizeInBytes);
    for (int64_t index = 0; index < 8 * kBitmapSizeInBytes; ++index) {
      result[index] = reader.IsSet();
      reader.Next();
    }
    return result;
  }

  // A pre-defined packed bitmap for use in test cases.
  const PackedBitmapType packed_bitmap_;

  // The expected unpacked bitmap that would be generated if each bit in
  // the entire source bitmap was correctly unpacked to bytes.
  const UnpackedBitmapType expected_unpacked_bitmap_;

  // Define a test constructor that populates the packed bitmap and the expected
  // unpacked bitmap.
  TestVisitBits()
      : packed_bitmap_(generate_packed_bitmap()),
        expected_unpacked_bitmap_(generate_unpacked_bitmap(packed_bitmap_)) {}
};

using VisitBitsTestTypes = ::testing::Types<VisitBitsFunctor, VisitBitsUnrolledFunctor>;
TYPED_TEST_SUITE(TestVisitBits, VisitBitsTestTypes);

/* Test bit-unpacking when reading less than eight bits from the input */
TYPED_TEST(TestVisitBits, NormalOperation) {
  typename TestFixture::UnpackedBitmapType unpacked_bitmap;
  const int64_t start_offsets[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 21, 31, 32};
  const int64_t lengths[] = {0,  1,  2,  3,  4,   5,   6,   7,   8,   9,   12,  16,
                             17, 21, 31, 32, 100, 201, 202, 203, 204, 205, 206, 207};
  const bool fill_values[] = {false, true};

  for (const bool fill_value : fill_values) {
    auto is_unmodified = [=](bool value) -> bool { return value == fill_value; };

    for (const int64_t start_offset : start_offsets) {
      for (const int64_t length : lengths) {
        std::string failure_info = std::string("fill value: ") +
                                   std::to_string(fill_value) +
                                   ", start offset: " + std::to_string(start_offset) +
                                   ", length: " + std::to_string(length);
        // Pre-fill the unpacked_bitmap array.
        unpacked_bitmap.fill(fill_value);

        // Attempt to read bits from the input bitmap into the unpacked_bitmap bitmap.
        using VisitBitsFunctor = TypeParam;
        VisitBitsFunctor()(this->packed_bitmap_.data(), start_offset, length,
                           unpacked_bitmap.data() + start_offset);

        // Verify that the correct values have been written in the [start_offset,
        // start_offset+length) range.
        EXPECT_TRUE(std::equal(unpacked_bitmap.begin() + start_offset,
                               unpacked_bitmap.begin() + start_offset + length,
                               this->expected_unpacked_bitmap_.begin() + start_offset))
            << "Invalid bytes unpacked when using " << failure_info;

        // Verify that the unpacked_bitmap array has not changed before or after
        // the [start_offset, start_offset+length) range.
        EXPECT_TRUE(std::all_of(unpacked_bitmap.begin(),
                                unpacked_bitmap.begin() + start_offset, is_unmodified))
            << "Unexpected modification to unpacked_bitmap array before written range "
               "when using "
            << failure_info;
        EXPECT_TRUE(std::all_of(unpacked_bitmap.begin() + start_offset + length,
                                unpacked_bitmap.end(), is_unmodified))
            << "Unexpected modification to unpacked_bitmap array after written range "
               "when using "
            << failure_info;
      }
    }
  }
}

struct BitmapOperation {
  virtual Result<std::shared_ptr<Buffer>> Call(MemoryPool* pool, const uint8_t* left,
                                               int64_t left_offset, const uint8_t* right,
                                               int64_t right_offset, int64_t length,
                                               int64_t out_offset) const = 0;

  virtual Status Call(const uint8_t* left, int64_t left_offset, const uint8_t* right,
                      int64_t right_offset, int64_t length, int64_t out_offset,
                      uint8_t* out_buffer) const = 0;

  virtual ~BitmapOperation() = default;
};

struct BitmapAndOp : public BitmapOperation {
  Result<std::shared_ptr<Buffer>> Call(MemoryPool* pool, const uint8_t* left,
                                       int64_t left_offset, const uint8_t* right,
                                       int64_t right_offset, int64_t length,
                                       int64_t out_offset) const override {
    return BitmapAnd(pool, left, left_offset, right, right_offset, length, out_offset);
  }

  Status Call(const uint8_t* left, int64_t left_offset, const uint8_t* right,
              int64_t right_offset, int64_t length, int64_t out_offset,
              uint8_t* out_buffer) const override {
    BitmapAnd(left, left_offset, right, right_offset, length, out_offset, out_buffer);
    return Status::OK();
  }
};

struct BitmapOrOp : public BitmapOperation {
  Result<std::shared_ptr<Buffer>> Call(MemoryPool* pool, const uint8_t* left,
                                       int64_t left_offset, const uint8_t* right,
                                       int64_t right_offset, int64_t length,
                                       int64_t out_offset) const override {
    return BitmapOr(pool, left, left_offset, right, right_offset, length, out_offset);
  }

  Status Call(const uint8_t* left, int64_t left_offset, const uint8_t* right,
              int64_t right_offset, int64_t length, int64_t out_offset,
              uint8_t* out_buffer) const override {
    BitmapOr(left, left_offset, right, right_offset, length, out_offset, out_buffer);
    return Status::OK();
  }
};

struct BitmapXorOp : public BitmapOperation {
  Result<std::shared_ptr<Buffer>> Call(MemoryPool* pool, const uint8_t* left,
                                       int64_t left_offset, const uint8_t* right,
                                       int64_t right_offset, int64_t length,
                                       int64_t out_offset) const override {
    return BitmapXor(pool, left, left_offset, right, right_offset, length, out_offset);
  }

  Status Call(const uint8_t* left, int64_t left_offset, const uint8_t* right,
              int64_t right_offset, int64_t length, int64_t out_offset,
              uint8_t* out_buffer) const override {
    BitmapXor(left, left_offset, right, right_offset, length, out_offset, out_buffer);
    return Status::OK();
  }
};

class BitmapOp : public TestBase {
 public:
  void TestAligned(const BitmapOperation& op, const std::vector<int>& left_bits,
                   const std::vector<int>& right_bits,
                   const std::vector<int>& result_bits) {
    std::shared_ptr<Buffer> left, right, out;
    int64_t length;

    for (int64_t left_offset : {0, 1, 3, 5, 7, 8, 13, 21, 38, 75, 120, 65536}) {
      BitmapFromVector(left_bits, left_offset, &left, &length);
      for (int64_t right_offset : {left_offset, left_offset + 8, left_offset + 40}) {
        BitmapFromVector(right_bits, right_offset, &right, &length);
        for (int64_t out_offset : {left_offset, left_offset + 16, left_offset + 24}) {
          ASSERT_OK_AND_ASSIGN(
              out, op.Call(default_memory_pool(), left->mutable_data(), left_offset,
                           right->mutable_data(), right_offset, length, out_offset));
          auto reader = internal::BitmapReader(out->mutable_data(), out_offset, length);
          ASSERT_READER_VALUES(reader, result_bits);

          // Clear out buffer and try non-allocating version
          std::memset(out->mutable_data(), 0, out->size());
          ASSERT_OK(op.Call(left->mutable_data(), left_offset, right->mutable_data(),
                            right_offset, length, out_offset, out->mutable_data()));
          reader = internal::BitmapReader(out->mutable_data(), out_offset, length);
          ASSERT_READER_VALUES(reader, result_bits);
        }
      }
    }
  }

  void TestUnaligned(const BitmapOperation& op, const std::vector<int>& left_bits,
                     const std::vector<int>& right_bits,
                     const std::vector<int>& result_bits) {
    std::shared_ptr<Buffer> left, right, out;
    int64_t length;
    auto offset_values = {0, 1, 3, 5, 7, 8, 13, 21, 38, 75, 120, 65536};

    for (int64_t left_offset : offset_values) {
      BitmapFromVector(left_bits, left_offset, &left, &length);

      for (int64_t right_offset : offset_values) {
        BitmapFromVector(right_bits, right_offset, &right, &length);

        for (int64_t out_offset : offset_values) {
          ASSERT_OK_AND_ASSIGN(
              out, op.Call(default_memory_pool(), left->mutable_data(), left_offset,
                           right->mutable_data(), right_offset, length, out_offset));
          auto reader = internal::BitmapReader(out->mutable_data(), out_offset, length);
          ASSERT_READER_VALUES(reader, result_bits);

          // Clear out buffer and try non-allocating version
          std::memset(out->mutable_data(), 0, out->size());
          ASSERT_OK(op.Call(left->mutable_data(), left_offset, right->mutable_data(),
                            right_offset, length, out_offset, out->mutable_data()));
          reader = internal::BitmapReader(out->mutable_data(), out_offset, length);
          ASSERT_READER_VALUES(reader, result_bits);
        }
      }
    }
  }
};

TEST_F(BitmapOp, And) {
  BitmapAndOp op;
  std::vector<int> left = {0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1};
  std::vector<int> right = {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0};
  std::vector<int> result = {0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};

  TestAligned(op, left, right, result);
  TestUnaligned(op, left, right, result);
}

TEST_F(BitmapOp, Or) {
  BitmapOrOp op;
  std::vector<int> left = {0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0};
  std::vector<int> right = {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0};
  std::vector<int> result = {0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0};

  TestAligned(op, left, right, result);
  TestUnaligned(op, left, right, result);
}

TEST_F(BitmapOp, Xor) {
  BitmapXorOp op;
  std::vector<int> left = {0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1};
  std::vector<int> right = {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0};
  std::vector<int> result = {0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1};

  TestAligned(op, left, right, result);
  TestUnaligned(op, left, right, result);
}

static inline int64_t SlowCountBits(const uint8_t* data, int64_t bit_offset,
                                    int64_t length) {
  int64_t count = 0;
  for (int64_t i = bit_offset; i < bit_offset + length; ++i) {
    if (BitUtil::GetBit(data, i)) {
      ++count;
    }
  }
  return count;
}

TEST(BitUtilTests, TestCountSetBits) {
  const int kBufferSize = 1000;
  alignas(8) uint8_t buffer[kBufferSize] = {0};
  const int buffer_bits = kBufferSize * 8;

  random_bytes(kBufferSize, 0, buffer);

  // Check start addresses with 64-bit alignment and without
  for (const uint8_t* data : {buffer, buffer + 1, buffer + 7}) {
    for (const int num_bits : {buffer_bits - 96, buffer_bits - 101, buffer_bits - 127}) {
      std::vector<int64_t> offsets = {
          0, 12, 16, 32, 37, 63, 64, 128, num_bits - 30, num_bits - 64};
      for (const int64_t offset : offsets) {
        int64_t result = CountSetBits(data, offset, num_bits - offset);
        int64_t expected = SlowCountBits(data, offset, num_bits - offset);

        ASSERT_EQ(expected, result);
      }
    }
  }
}

TEST(BitUtilTests, TestSetBitsTo) {
  using BitUtil::SetBitsTo;
  for (const auto fill_byte_int : {0x00, 0xff}) {
    const uint8_t fill_byte = static_cast<uint8_t>(fill_byte_int);
    {
      // test set within a byte
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      SetBitsTo(bitmap, 2, 2, true);
      SetBitsTo(bitmap, 4, 2, false);
      ASSERT_BYTES_EQ(bitmap, {static_cast<uint8_t>((fill_byte & ~0x3C) | 0xC)});
    }
    {
      // test straddling a single byte boundary
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      SetBitsTo(bitmap, 4, 7, true);
      SetBitsTo(bitmap, 11, 7, false);
      ASSERT_BYTES_EQ(bitmap, {static_cast<uint8_t>((fill_byte & 0xF) | 0xF0), 0x7,
                               static_cast<uint8_t>(fill_byte & ~0x3)});
    }
    {
      // test byte aligned end
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      SetBitsTo(bitmap, 4, 4, true);
      SetBitsTo(bitmap, 8, 8, false);
      ASSERT_BYTES_EQ(bitmap,
                      {static_cast<uint8_t>((fill_byte & 0xF) | 0xF0), 0x00, fill_byte});
    }
    {
      // test byte aligned end, multiple bytes
      uint8_t bitmap[] = {fill_byte, fill_byte, fill_byte, fill_byte};
      SetBitsTo(bitmap, 0, 24, false);
      uint8_t false_byte = static_cast<uint8_t>(0);
      ASSERT_BYTES_EQ(bitmap, {false_byte, false_byte, false_byte, fill_byte});
    }
  }
}

TEST(BitUtilTests, TestCopyBitmap) {
  const int kBufferSize = 1000;

  ASSERT_OK_AND_ASSIGN(auto buffer, AllocateBuffer(kBufferSize));
  memset(buffer->mutable_data(), 0, kBufferSize);
  random_bytes(kBufferSize, 0, buffer->mutable_data());

  const uint8_t* src = buffer->data();

  std::vector<int64_t> lengths = {kBufferSize * 8 - 4, kBufferSize * 8};
  std::vector<int64_t> offsets = {0, 12, 16, 32, 37, 63, 64, 128};
  for (int64_t num_bits : lengths) {
    for (int64_t offset : offsets) {
      const int64_t copy_length = num_bits - offset;

      std::shared_ptr<Buffer> copy;
      ASSERT_OK_AND_ASSIGN(copy,
                           CopyBitmap(default_memory_pool(), src, offset, copy_length));

      for (int64_t i = 0; i < copy_length; ++i) {
        ASSERT_EQ(BitUtil::GetBit(src, i + offset), BitUtil::GetBit(copy->data(), i));
      }
    }
  }
}

TEST(BitUtilTests, TestCopyBitmapPreAllocated) {
  const int kBufferSize = 1000;
  std::vector<int64_t> lengths = {kBufferSize * 8 - 4, kBufferSize * 8};
  std::vector<int64_t> offsets = {0, 12, 16, 32, 37, 63, 64, 128};

  ASSERT_OK_AND_ASSIGN(auto buffer, AllocateBuffer(kBufferSize));
  memset(buffer->mutable_data(), 0, kBufferSize);
  random_bytes(kBufferSize, 0, buffer->mutable_data());
  const uint8_t* src = buffer->data();

  // Add 16 byte padding on both sides
  ASSERT_OK_AND_ASSIGN(auto other_buffer, AllocateBuffer(kBufferSize + 32));
  memset(other_buffer->mutable_data(), 0, kBufferSize + 32);
  random_bytes(kBufferSize + 32, 0, other_buffer->mutable_data());
  const uint8_t* other = other_buffer->data();

  for (int64_t num_bits : lengths) {
    for (int64_t offset : offsets) {
      for (int64_t dest_offset : offsets) {
        const int64_t copy_length = num_bits - offset;

        ASSERT_OK_AND_ASSIGN(auto copy, AllocateBuffer(other_buffer->size()));
        memcpy(copy->mutable_data(), other_buffer->data(), other_buffer->size());
        CopyBitmap(src, offset, copy_length, copy->mutable_data(), dest_offset);

        for (int64_t i = 0; i < dest_offset; ++i) {
          ASSERT_EQ(BitUtil::GetBit(other, i), BitUtil::GetBit(copy->data(), i));
        }
        for (int64_t i = 0; i < copy_length; ++i) {
          ASSERT_EQ(BitUtil::GetBit(src, i + offset),
                    BitUtil::GetBit(copy->data(), i + dest_offset));
        }
        for (int64_t i = dest_offset + copy_length; i < (other_buffer->size() * 8); ++i) {
          ASSERT_EQ(BitUtil::GetBit(other, i), BitUtil::GetBit(copy->data(), i));
        }
      }
    }
  }
}

TEST(BitUtilTests, TestCopyAndInvertBitmapPreAllocated) {
  const int kBufferSize = 1000;
  std::vector<int64_t> lengths = {kBufferSize * 8 - 4, kBufferSize * 8};
  std::vector<int64_t> offsets = {0, 12, 16, 32, 37, 63, 64, 128};

  ASSERT_OK_AND_ASSIGN(auto buffer, AllocateBuffer(kBufferSize));
  memset(buffer->mutable_data(), 0, kBufferSize);
  random_bytes(kBufferSize, 0, buffer->mutable_data());
  const uint8_t* src = buffer->data();

  // Add 16 byte padding on both sides
  ASSERT_OK_AND_ASSIGN(auto other_buffer, AllocateBuffer(kBufferSize + 32));
  memset(other_buffer->mutable_data(), 0, kBufferSize + 32);
  random_bytes(kBufferSize + 32, 0, other_buffer->mutable_data());
  const uint8_t* other = other_buffer->data();

  for (int64_t num_bits : lengths) {
    for (int64_t offset : offsets) {
      for (int64_t dest_offset : offsets) {
        const int64_t copy_length = num_bits - offset;

        ASSERT_OK_AND_ASSIGN(auto copy, AllocateBuffer(other_buffer->size()));
        memcpy(copy->mutable_data(), other_buffer->data(), other_buffer->size());
        InvertBitmap(src, offset, copy_length, copy->mutable_data(), dest_offset);

        for (int64_t i = 0; i < dest_offset; ++i) {
          ASSERT_EQ(BitUtil::GetBit(other, i), BitUtil::GetBit(copy->data(), i));
        }
        for (int64_t i = 0; i < copy_length; ++i) {
          ASSERT_EQ(BitUtil::GetBit(src, i + offset),
                    !BitUtil::GetBit(copy->data(), i + dest_offset));
        }
        for (int64_t i = dest_offset + copy_length; i < (other_buffer->size() * 8); ++i) {
          ASSERT_EQ(BitUtil::GetBit(other, i), BitUtil::GetBit(copy->data(), i));
        }
      }
    }
  }
}

TEST(BitUtil, CeilDiv) {
  EXPECT_EQ(BitUtil::CeilDiv(0, 1), 0);
  EXPECT_EQ(BitUtil::CeilDiv(1, 1), 1);
  EXPECT_EQ(BitUtil::CeilDiv(1, 2), 1);
  EXPECT_EQ(BitUtil::CeilDiv(0, 8), 0);
  EXPECT_EQ(BitUtil::CeilDiv(1, 8), 1);
  EXPECT_EQ(BitUtil::CeilDiv(7, 8), 1);
  EXPECT_EQ(BitUtil::CeilDiv(8, 8), 1);
  EXPECT_EQ(BitUtil::CeilDiv(9, 8), 2);
  EXPECT_EQ(BitUtil::CeilDiv(9, 9), 1);
  EXPECT_EQ(BitUtil::CeilDiv(10000000000, 10), 1000000000);
  EXPECT_EQ(BitUtil::CeilDiv(10, 10000000000), 1);
  EXPECT_EQ(BitUtil::CeilDiv(100000000000, 10000000000), 10);

  // test overflow
  int64_t value = std::numeric_limits<int64_t>::max() - 1;
  int64_t divisor = std::numeric_limits<int64_t>::max();
  EXPECT_EQ(BitUtil::CeilDiv(value, divisor), 1);

  value = std::numeric_limits<int64_t>::max();
  EXPECT_EQ(BitUtil::CeilDiv(value, divisor), 1);
}

TEST(BitUtil, RoundUp) {
  EXPECT_EQ(BitUtil::RoundUp(0, 1), 0);
  EXPECT_EQ(BitUtil::RoundUp(1, 1), 1);
  EXPECT_EQ(BitUtil::RoundUp(1, 2), 2);
  EXPECT_EQ(BitUtil::RoundUp(6, 2), 6);
  EXPECT_EQ(BitUtil::RoundUp(0, 3), 0);
  EXPECT_EQ(BitUtil::RoundUp(7, 3), 9);
  EXPECT_EQ(BitUtil::RoundUp(9, 9), 9);
  EXPECT_EQ(BitUtil::RoundUp(10000000001, 10), 10000000010);
  EXPECT_EQ(BitUtil::RoundUp(10, 10000000000), 10000000000);
  EXPECT_EQ(BitUtil::RoundUp(100000000000, 10000000000), 100000000000);

  // test overflow
  int64_t value = std::numeric_limits<int64_t>::max() - 1;
  int64_t divisor = std::numeric_limits<int64_t>::max();
  EXPECT_EQ(BitUtil::RoundUp(value, divisor), divisor);

  value = std::numeric_limits<int64_t>::max();
  EXPECT_EQ(BitUtil::RoundUp(value, divisor), divisor);
}

TEST(BitUtil, RoundDown) {
  EXPECT_EQ(BitUtil::RoundDown(0, 1), 0);
  EXPECT_EQ(BitUtil::RoundDown(1, 1), 1);
  EXPECT_EQ(BitUtil::RoundDown(1, 2), 0);
  EXPECT_EQ(BitUtil::RoundDown(6, 2), 6);
  EXPECT_EQ(BitUtil::RoundDown(5, 7), 0);
  EXPECT_EQ(BitUtil::RoundDown(10, 7), 7);
  EXPECT_EQ(BitUtil::RoundDown(7, 3), 6);
  EXPECT_EQ(BitUtil::RoundDown(9, 9), 9);
  EXPECT_EQ(BitUtil::RoundDown(10000000001, 10), 10000000000);
  EXPECT_EQ(BitUtil::RoundDown(10, 10000000000), 0);
  EXPECT_EQ(BitUtil::RoundDown(100000000000, 10000000000), 100000000000);

  for (int i = 0; i < 100; i++) {
    for (int j = 1; j < 100; j++) {
      EXPECT_EQ(BitUtil::RoundDown(i, j), i - (i % j));
    }
  }
}

TEST(BitUtil, CoveringBytes) {
  EXPECT_EQ(BitUtil::CoveringBytes(0, 8), 1);
  EXPECT_EQ(BitUtil::CoveringBytes(0, 9), 2);
  EXPECT_EQ(BitUtil::CoveringBytes(1, 7), 1);
  EXPECT_EQ(BitUtil::CoveringBytes(1, 8), 2);
  EXPECT_EQ(BitUtil::CoveringBytes(2, 19), 3);
  EXPECT_EQ(BitUtil::CoveringBytes(7, 18), 4);
}

TEST(BitUtil, TrailingBits) {
  EXPECT_EQ(BitUtil::TrailingBits(0xFF, 0), 0);
  EXPECT_EQ(BitUtil::TrailingBits(0xFF, 1), 1);
  EXPECT_EQ(BitUtil::TrailingBits(0xFF, 64), 0xFF);
  EXPECT_EQ(BitUtil::TrailingBits(0xFF, 100), 0xFF);
  EXPECT_EQ(BitUtil::TrailingBits(0, 1), 0);
  EXPECT_EQ(BitUtil::TrailingBits(0, 64), 0);
  EXPECT_EQ(BitUtil::TrailingBits(1LL << 63, 0), 0);
  EXPECT_EQ(BitUtil::TrailingBits(1LL << 63, 63), 0);
  EXPECT_EQ(BitUtil::TrailingBits(1LL << 63, 64), 1LL << 63);
}

TEST(BitUtil, ByteSwap) {
  EXPECT_EQ(BitUtil::ByteSwap(static_cast<uint32_t>(0)), 0);
  EXPECT_EQ(BitUtil::ByteSwap(static_cast<uint32_t>(0x11223344)), 0x44332211);

  EXPECT_EQ(BitUtil::ByteSwap(static_cast<int32_t>(0)), 0);
  EXPECT_EQ(BitUtil::ByteSwap(static_cast<int32_t>(0x11223344)), 0x44332211);

  EXPECT_EQ(BitUtil::ByteSwap(static_cast<uint64_t>(0)), 0);
  EXPECT_EQ(BitUtil::ByteSwap(static_cast<uint64_t>(0x1122334455667788)),
            0x8877665544332211);

  EXPECT_EQ(BitUtil::ByteSwap(static_cast<int64_t>(0)), 0);
  EXPECT_EQ(BitUtil::ByteSwap(static_cast<int64_t>(0x1122334455667788)),
            0x8877665544332211);

  EXPECT_EQ(BitUtil::ByteSwap(static_cast<int16_t>(0)), 0);
  EXPECT_EQ(BitUtil::ByteSwap(static_cast<int16_t>(0x1122)), 0x2211);

  EXPECT_EQ(BitUtil::ByteSwap(static_cast<uint16_t>(0)), 0);
  EXPECT_EQ(BitUtil::ByteSwap(static_cast<uint16_t>(0x1122)), 0x2211);
}

TEST(BitUtil, Log2) {
  EXPECT_EQ(BitUtil::Log2(1), 0);
  EXPECT_EQ(BitUtil::Log2(2), 1);
  EXPECT_EQ(BitUtil::Log2(3), 2);
  EXPECT_EQ(BitUtil::Log2(4), 2);
  EXPECT_EQ(BitUtil::Log2(5), 3);
  EXPECT_EQ(BitUtil::Log2(8), 3);
  EXPECT_EQ(BitUtil::Log2(9), 4);
  EXPECT_EQ(BitUtil::Log2(INT_MAX), 31);
  EXPECT_EQ(BitUtil::Log2(UINT_MAX), 32);
  EXPECT_EQ(BitUtil::Log2(ULLONG_MAX), 64);
}

TEST(BitUtil, NumRequiredBits) {
  EXPECT_EQ(BitUtil::NumRequiredBits(0), 0);
  EXPECT_EQ(BitUtil::NumRequiredBits(1), 1);
  EXPECT_EQ(BitUtil::NumRequiredBits(2), 2);
  EXPECT_EQ(BitUtil::NumRequiredBits(3), 2);
  EXPECT_EQ(BitUtil::NumRequiredBits(4), 3);
  EXPECT_EQ(BitUtil::NumRequiredBits(5), 3);
  EXPECT_EQ(BitUtil::NumRequiredBits(7), 3);
  EXPECT_EQ(BitUtil::NumRequiredBits(8), 4);
  EXPECT_EQ(BitUtil::NumRequiredBits(9), 4);
  EXPECT_EQ(BitUtil::NumRequiredBits(UINT_MAX - 1), 32);
  EXPECT_EQ(BitUtil::NumRequiredBits(UINT_MAX), 32);
  EXPECT_EQ(BitUtil::NumRequiredBits(static_cast<uint64_t>(UINT_MAX) + 1), 33);
  EXPECT_EQ(BitUtil::NumRequiredBits(ULLONG_MAX / 2), 63);
  EXPECT_EQ(BitUtil::NumRequiredBits(ULLONG_MAX / 2 + 1), 64);
  EXPECT_EQ(BitUtil::NumRequiredBits(ULLONG_MAX - 1), 64);
  EXPECT_EQ(BitUtil::NumRequiredBits(ULLONG_MAX), 64);
}

#define U32(x) static_cast<uint32_t>(x)
#define U64(x) static_cast<uint64_t>(x)
#define S64(x) static_cast<int64_t>(x)

TEST(BitUtil, CountLeadingZeros) {
  EXPECT_EQ(BitUtil::CountLeadingZeros(U32(0)), 32);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U32(1)), 31);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U32(2)), 30);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U32(3)), 30);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U32(4)), 29);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U32(7)), 29);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U32(8)), 28);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U32(UINT_MAX / 2)), 1);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U32(UINT_MAX / 2 + 1)), 0);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U32(UINT_MAX)), 0);

  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(0)), 64);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(1)), 63);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(2)), 62);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(3)), 62);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(4)), 61);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(7)), 61);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(8)), 60);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(UINT_MAX)), 32);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(UINT_MAX) + 1), 31);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(ULLONG_MAX / 2)), 1);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(ULLONG_MAX / 2 + 1)), 0);
  EXPECT_EQ(BitUtil::CountLeadingZeros(U64(ULLONG_MAX)), 0);
}

TEST(BitUtil, CountTrailingZeros) {
  EXPECT_EQ(BitUtil::CountTrailingZeros(U32(0)), 32);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U32(1) << 31), 31);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U32(1) << 30), 30);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U32(1) << 29), 29);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U32(1) << 28), 28);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U32(8)), 3);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U32(4)), 2);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U32(2)), 1);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U32(1)), 0);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U32(ULONG_MAX)), 0);

  EXPECT_EQ(BitUtil::CountTrailingZeros(U64(0)), 64);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U64(1) << 63), 63);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U64(1) << 62), 62);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U64(1) << 61), 61);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U64(1) << 60), 60);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U64(8)), 3);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U64(4)), 2);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U64(2)), 1);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U64(1)), 0);
  EXPECT_EQ(BitUtil::CountTrailingZeros(U64(ULLONG_MAX)), 0);
}

TEST(BitUtil, RoundUpToPowerOf2) {
  EXPECT_EQ(BitUtil::RoundUpToPowerOf2(S64(7), 8), 8);
  EXPECT_EQ(BitUtil::RoundUpToPowerOf2(S64(8), 8), 8);
  EXPECT_EQ(BitUtil::RoundUpToPowerOf2(S64(9), 8), 16);

  EXPECT_EQ(BitUtil::RoundUpToPowerOf2(U64(7), 8), 8);
  EXPECT_EQ(BitUtil::RoundUpToPowerOf2(U64(8), 8), 8);
  EXPECT_EQ(BitUtil::RoundUpToPowerOf2(U64(9), 8), 16);
}

#undef U32
#undef U64
#undef S64

static void TestZigZag(int32_t v) {
  uint8_t buffer[BitUtil::BitReader::kMaxVlqByteLength] = {};
  BitUtil::BitWriter writer(buffer, sizeof(buffer));
  BitUtil::BitReader reader(buffer, sizeof(buffer));
  writer.PutZigZagVlqInt(v);
  int32_t result;
  EXPECT_TRUE(reader.GetZigZagVlqInt(&result));
  EXPECT_EQ(v, result);
}

TEST(BitStreamUtil, ZigZag) {
  TestZigZag(0);
  TestZigZag(1);
  TestZigZag(1234);
  TestZigZag(-1);
  TestZigZag(-1234);
  TestZigZag(std::numeric_limits<int32_t>::max());
  TestZigZag(-std::numeric_limits<int32_t>::max());
}

TEST(BitUtil, RoundTripLittleEndianTest) {
  uint64_t value = 0xFF;

#if ARROW_LITTLE_ENDIAN
  uint64_t expected = value;
#else
  uint64_t expected = std::numeric_limits<uint64_t>::max() << 56;
#endif

  uint64_t little_endian_result = BitUtil::ToLittleEndian(value);
  ASSERT_EQ(expected, little_endian_result);

  uint64_t from_little_endian = BitUtil::FromLittleEndian(little_endian_result);
  ASSERT_EQ(value, from_little_endian);
}

TEST(BitUtil, RoundTripBigEndianTest) {
  uint64_t value = 0xFF;

#if ARROW_LITTLE_ENDIAN
  uint64_t expected = std::numeric_limits<uint64_t>::max() << 56;
#else
  uint64_t expected = value;
#endif

  uint64_t big_endian_result = BitUtil::ToBigEndian(value);
  ASSERT_EQ(expected, big_endian_result);

  uint64_t from_big_endian = BitUtil::FromBigEndian(big_endian_result);
  ASSERT_EQ(value, from_big_endian);
}

TEST(BitUtil, BitsetStack) {
  BitsetStack stack;
  ASSERT_EQ(stack.TopSize(), 0);
  stack.Push(3, false);
  ASSERT_EQ(stack.TopSize(), 3);
  stack[1] = true;
  stack.Push(5, true);
  ASSERT_EQ(stack.TopSize(), 5);
  stack[1] = false;
  for (int i = 0; i != 5; ++i) {
    ASSERT_EQ(stack[i], i != 1);
  }
  stack.Pop();
  ASSERT_EQ(stack.TopSize(), 3);
  for (int i = 0; i != 3; ++i) {
    ASSERT_EQ(stack[i], i == 1);
  }
  stack.Pop();
  ASSERT_EQ(stack.TopSize(), 0);
}

// test the basic assumption of word level Bitmap::Visit
TEST(Bitmap, ShiftingWordsOptimization) {
  // single word
  {
    uint64_t word;
    auto bytes = reinterpret_cast<uint8_t*>(&word);
    constexpr size_t kBitWidth = sizeof(word) * 8;

    for (int seed = 0; seed < 64; ++seed) {
      random_bytes(sizeof(word), seed, bytes);

      // bits are accessible through simple bit shifting of the word
      for (size_t i = 0; i < kBitWidth; ++i) {
        ASSERT_EQ(BitUtil::GetBit(bytes, i), bool((word >> i) & 1));
      }

      // bit offset can therefore be accommodated by shifting the word
      for (size_t offset = 0; offset < (kBitWidth * 3) / 4; ++offset) {
        uint64_t shifted_word = word >> offset;
        auto shifted_bytes = reinterpret_cast<uint8_t*>(&shifted_word);
        ASSERT_TRUE(
            internal::BitmapEquals(bytes, offset, shifted_bytes, 0, kBitWidth - offset));
      }
    }
  }

  // two words
  {
    uint64_t words[2];
    auto bytes = reinterpret_cast<uint8_t*>(words);
    constexpr size_t kBitWidth = sizeof(words[0]) * 8;

    for (int seed = 0; seed < 64; ++seed) {
      random_bytes(sizeof(words), seed, bytes);

      // bits are accessible through simple bit shifting of a word
      for (size_t i = 0; i < kBitWidth; ++i) {
        ASSERT_EQ(BitUtil::GetBit(bytes, i), bool((words[0] >> i) & 1));
      }
      for (size_t i = 0; i < kBitWidth; ++i) {
        ASSERT_EQ(BitUtil::GetBit(bytes, i + kBitWidth), bool((words[1] >> i) & 1));
      }

      // bit offset can therefore be accommodated by shifting the word
      for (size_t offset = 1; offset < (kBitWidth * 3) / 4; offset += 3) {
        uint64_t shifted_words[2];
        shifted_words[0] = words[0] >> offset | (words[1] << (kBitWidth - offset));
        shifted_words[1] = words[1] >> offset;
        auto shifted_bytes = reinterpret_cast<uint8_t*>(shifted_words);

        // from offset to unshifted word boundary
        ASSERT_TRUE(
            internal::BitmapEquals(bytes, offset, shifted_bytes, 0, kBitWidth - offset));

        // from unshifted word boundary to shifted word boundary
        ASSERT_TRUE(internal::BitmapEquals(bytes, kBitWidth, shifted_bytes,
                                           kBitWidth - offset, offset));

        // from shifted word boundary to end
        ASSERT_TRUE(internal::BitmapEquals(bytes, kBitWidth + offset, shifted_bytes,
                                           kBitWidth, kBitWidth - offset));
      }
    }
  }
}

namespace internal {

static Bitmap Copy(const Bitmap& bitmap, std::shared_ptr<Buffer> storage) {
  int64_t i = 0;
  Bitmap bitmaps[] = {bitmap};
  auto min_offset = Bitmap::VisitWords(bitmaps, [&](std::array<uint64_t, 1> uint64s) {
    reinterpret_cast<uint64_t*>(storage->mutable_data())[i++] = uint64s[0];
  });
  return Bitmap(std::move(storage), min_offset, bitmap.length());
}

// reconstruct a bitmap from a word-wise visit
TEST(Bitmap, VisitWords) {
  constexpr int64_t nbytes = 1 << 10;
  std::shared_ptr<Buffer> buffer, actual_buffer;
  for (std::shared_ptr<Buffer>* b : {&buffer, &actual_buffer}) {
    ASSERT_OK_AND_ASSIGN(*b, AllocateBuffer(nbytes));
    memset((*b)->mutable_data(), 0, nbytes);
  }
  random_bytes(nbytes, 0, buffer->mutable_data());

  constexpr int64_t kBitWidth = 8 * sizeof(uint64_t);

  for (int64_t offset : {0, 1, 2, 5, 17}) {
    for (int64_t num_bits :
         {int64_t(13), int64_t(9), kBitWidth - 1, kBitWidth, kBitWidth + 1,
          nbytes * 8 - offset, nbytes * 6, nbytes * 4}) {
      Bitmap actual = Copy({buffer, offset, num_bits}, actual_buffer);
      ASSERT_EQ(actual, Bitmap(buffer->data(), offset, num_bits))
          << "offset:" << offset << "  bits:" << num_bits << std::endl
          << Bitmap(actual_buffer, 0, num_bits).Diff({buffer, offset, num_bits});
    }
  }
}

#ifndef ARROW_VALGRIND

// This test reads uninitialized memory
TEST(Bitmap, VisitPartialWords) {
  uint64_t words[2];
  constexpr auto nbytes = sizeof(words);
  constexpr auto nbits = nbytes * 8;

  auto buffer = Buffer::Wrap(words, 2);
  Bitmap bitmap(buffer, 0, nbits);
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<Buffer> storage, AllocateBuffer(nbytes));

  // words partially outside the buffer are not accessible, but they are loaded bitwise
  auto first_byte_was_missing = Bitmap(SliceBuffer(buffer, 1), 0, nbits - 8);
  ASSERT_EQ(Copy(first_byte_was_missing, storage), bitmap.Slice(8));

  auto last_byte_was_missing = Bitmap(SliceBuffer(buffer, 0, nbytes - 1), 0, nbits - 8);
  ASSERT_EQ(Copy(last_byte_was_missing, storage), bitmap.Slice(0, nbits - 8));
}

#endif  // ARROW_VALGRIND

// compute bitwise AND of bitmaps using word-wise visit
TEST(Bitmap, VisitWordsAnd) {
  constexpr int64_t nbytes = 1 << 10;
  std::shared_ptr<Buffer> buffer, actual_buffer, expected_buffer;
  for (std::shared_ptr<Buffer>* b : {&buffer, &actual_buffer, &expected_buffer}) {
    ASSERT_OK_AND_ASSIGN(*b, AllocateBuffer(nbytes));
    memset((*b)->mutable_data(), 0, nbytes);
  }
  random_bytes(nbytes, 0, buffer->mutable_data());

  constexpr int64_t kBitWidth = 8 * sizeof(uint64_t);

  for (int64_t left_offset :
       {0, 1, 2, 5, 17, int(kBitWidth - 1), int(kBitWidth + 1), int(kBitWidth + 17)}) {
    for (int64_t right_offset = 0; right_offset < left_offset; ++right_offset) {
      for (int64_t num_bits :
           {int64_t(13), int64_t(9), kBitWidth - 1, kBitWidth, kBitWidth + 1,
            2 * kBitWidth - 1, 2 * kBitWidth, 2 * kBitWidth + 1, nbytes * 8 - left_offset,
            3 * kBitWidth - 1, 3 * kBitWidth, 3 * kBitWidth + 1, nbytes * 6,
            nbytes * 4}) {
        Bitmap bitmaps[] = {{buffer, left_offset, num_bits},
                            {buffer, right_offset, num_bits}};

        int64_t i = 0;
        auto min_offset =
            Bitmap::VisitWords(bitmaps, [&](std::array<uint64_t, 2> uint64s) {
              reinterpret_cast<uint64_t*>(actual_buffer->mutable_data())[i++] =
                  uint64s[0] & uint64s[1];
            });

        BitmapAnd(bitmaps[0].buffer()->data(), bitmaps[0].offset(),
                  bitmaps[1].buffer()->data(), bitmaps[1].offset(), bitmaps[0].length(),
                  0, expected_buffer->mutable_data());

        ASSERT_TRUE(BitmapEquals(actual_buffer->data(), min_offset,
                                 expected_buffer->data(), 0, num_bits))
            << "left_offset:" << left_offset << "  bits:" << num_bits
            << "  right_offset:" << right_offset << std::endl
            << Bitmap(actual_buffer, 0, num_bits).Diff({expected_buffer, 0, num_bits});
      }
    }
  }
}
}  // namespace internal
}  // namespace arrow
