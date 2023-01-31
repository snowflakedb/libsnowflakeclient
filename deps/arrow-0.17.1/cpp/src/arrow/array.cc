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

#include "arrow/array.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <type_traits>
#include <utility>

#include "arrow/array/concatenate.h"
#include "arrow/array/validate.h"
#include "arrow/buffer.h"
#include "arrow/buffer_builder.h"
#include "arrow/compare.h"
#include "arrow/extension_type.h"
#include "arrow/pretty_print.h"
#include "arrow/status.h"
#include "arrow/type.h"
#include "arrow/type_traits.h"
#include "arrow/util/atomic_shared_ptr.h"
#include "arrow/util/bit_util.h"
#include "arrow/util/checked_cast.h"
#include "arrow/util/decimal.h"
#include "arrow/util/logging.h"
#include "arrow/util/macros.h"
#include "arrow/visitor.h"
#include "arrow/visitor_inline.h"

namespace arrow {

using internal::BitmapAnd;
using internal::checked_cast;
using internal::checked_pointer_cast;
using internal::CopyBitmap;
using internal::CountSetBits;

std::shared_ptr<ArrayData> ArrayData::Make(const std::shared_ptr<DataType>& type,
                                           int64_t length,
                                           std::vector<std::shared_ptr<Buffer>> buffers,
                                           int64_t null_count, int64_t offset) {
  // In case there are no nulls, don't keep an allocated null bitmap around
  if (buffers.size() > 0 && null_count == 0) {
    buffers[0] = nullptr;
  }
  return std::make_shared<ArrayData>(type, length, std::move(buffers), null_count,
                                     offset);
}

std::shared_ptr<ArrayData> ArrayData::Make(
    const std::shared_ptr<DataType>& type, int64_t length,
    std::vector<std::shared_ptr<Buffer>> buffers,
    std::vector<std::shared_ptr<ArrayData>> child_data, int64_t null_count,
    int64_t offset) {
  // In case there are no nulls, don't keep an allocated null bitmap around
  if (buffers.size() > 0 && null_count == 0) {
    buffers[0] = nullptr;
  }
  return std::make_shared<ArrayData>(type, length, std::move(buffers),
                                     std::move(child_data), null_count, offset);
}

std::shared_ptr<ArrayData> ArrayData::Make(
    const std::shared_ptr<DataType>& type, int64_t length,
    std::vector<std::shared_ptr<Buffer>> buffers,
    std::vector<std::shared_ptr<ArrayData>> child_data, std::shared_ptr<Array> dictionary,
    int64_t null_count, int64_t offset) {
  if (buffers.size() > 0 && null_count == 0) {
    buffers[0] = nullptr;
  }
  auto data = std::make_shared<ArrayData>(type, length, std::move(buffers),
                                          std::move(child_data), null_count, offset);
  data->dictionary = std::move(dictionary);
  return data;
}

std::shared_ptr<ArrayData> ArrayData::Make(const std::shared_ptr<DataType>& type,
                                           int64_t length, int64_t null_count,
                                           int64_t offset) {
  return std::make_shared<ArrayData>(type, length, null_count, offset);
}

ArrayData ArrayData::Slice(int64_t off, int64_t len) const {
  ARROW_CHECK_LE(off, length) << "Slice offset greater than array length";
  len = std::min(length - off, len);
  off += offset;

  auto copy = *this;
  copy.length = len;
  copy.offset = off;
  if (null_count == length) {
    copy.null_count = len;
  } else {
    copy.null_count = null_count != 0 ? kUnknownNullCount : 0;
  }
  return copy;
}

int64_t ArrayData::GetNullCount() const {
  int64_t precomputed = this->null_count.load();
  if (ARROW_PREDICT_FALSE(precomputed == kUnknownNullCount)) {
    if (this->buffers[0]) {
      precomputed = this->length -
                    CountSetBits(this->buffers[0]->data(), this->offset, this->length);
    } else {
      precomputed = 0;
    }
    this->null_count.store(precomputed);
  }
  return precomputed;
}

// ----------------------------------------------------------------------
// Base array class

int64_t Array::null_count() const { return data_->GetNullCount(); }

std::string Array::Diff(const Array& other) const {
  std::stringstream diff;
  ARROW_IGNORE_EXPR(Equals(other, EqualOptions().diff_sink(&diff)));
  return diff.str();
}

bool Array::Equals(const Array& arr, const EqualOptions& opts) const {
  return ArrayEquals(*this, arr, opts);
}

bool Array::Equals(const std::shared_ptr<Array>& arr, const EqualOptions& opts) const {
  if (!arr) {
    return false;
  }
  return Equals(*arr, opts);
}

bool Array::ApproxEquals(const Array& arr, const EqualOptions& opts) const {
  return ArrayApproxEquals(*this, arr, opts);
}

bool Array::ApproxEquals(const std::shared_ptr<Array>& arr,
                         const EqualOptions& opts) const {
  if (!arr) {
    return false;
  }
  return ApproxEquals(*arr, opts);
}

bool Array::RangeEquals(const Array& other, int64_t start_idx, int64_t end_idx,
                        int64_t other_start_idx) const {
  return ArrayRangeEquals(*this, other, start_idx, end_idx, other_start_idx);
}

bool Array::RangeEquals(const std::shared_ptr<Array>& other, int64_t start_idx,
                        int64_t end_idx, int64_t other_start_idx) const {
  if (!other) {
    return false;
  }
  return ArrayRangeEquals(*this, *other, start_idx, end_idx, other_start_idx);
}

bool Array::RangeEquals(int64_t start_idx, int64_t end_idx, int64_t other_start_idx,
                        const Array& other) const {
  return ArrayRangeEquals(*this, other, start_idx, end_idx, other_start_idx);
}

bool Array::RangeEquals(int64_t start_idx, int64_t end_idx, int64_t other_start_idx,
                        const std::shared_ptr<Array>& other) const {
  if (!other) {
    return false;
  }
  return ArrayRangeEquals(*this, *other, start_idx, end_idx, other_start_idx);
}

std::shared_ptr<Array> Array::Slice(int64_t offset, int64_t length) const {
  return MakeArray(std::make_shared<ArrayData>(data_->Slice(offset, length)));
}

std::shared_ptr<Array> Array::Slice(int64_t offset) const {
  int64_t slice_length = data_->length - offset;
  return Slice(offset, slice_length);
}

std::string Array::ToString() const {
  std::stringstream ss;
  ARROW_CHECK_OK(PrettyPrint(*this, 0, &ss));
  return ss.str();
}

// ----------------------------------------------------------------------
// NullArray

NullArray::NullArray(int64_t length) {
  SetData(ArrayData::Make(null(), length, {nullptr}, length));
}

// ----------------------------------------------------------------------
// Primitive array base

PrimitiveArray::PrimitiveArray(const std::shared_ptr<DataType>& type, int64_t length,
                               const std::shared_ptr<Buffer>& data,
                               const std::shared_ptr<Buffer>& null_bitmap,
                               int64_t null_count, int64_t offset) {
  SetData(ArrayData::Make(type, length, {null_bitmap, data}, null_count, offset));
}

// ----------------------------------------------------------------------
// BooleanArray

BooleanArray::BooleanArray(const std::shared_ptr<ArrayData>& data)
    : PrimitiveArray(data) {
  ARROW_CHECK_EQ(data->type->id(), Type::BOOL);
}

BooleanArray::BooleanArray(int64_t length, const std::shared_ptr<Buffer>& data,
                           const std::shared_ptr<Buffer>& null_bitmap, int64_t null_count,
                           int64_t offset)
    : PrimitiveArray(boolean(), length, data, null_bitmap, null_count, offset) {}

// ----------------------------------------------------------------------
// ListArray / LargeListArray

namespace {

template <typename TYPE>
Status CleanListOffsets(const Array& offsets, MemoryPool* pool,
                        std::shared_ptr<Buffer>* offset_buf_out,
                        std::shared_ptr<Buffer>* validity_buf_out) {
  using offset_type = typename TYPE::offset_type;
  using OffsetArrowType = typename CTypeTraits<offset_type>::ArrowType;
  using OffsetArrayType = typename TypeTraits<OffsetArrowType>::ArrayType;

  const auto& typed_offsets = checked_cast<const OffsetArrayType&>(offsets);
  const int64_t num_offsets = offsets.length();

  if (offsets.null_count() > 0) {
    if (!offsets.IsValid(num_offsets - 1)) {
      return Status::Invalid("Last list offset should be non-null");
    }

    ARROW_ASSIGN_OR_RAISE(auto clean_offsets,
                          AllocateBuffer(num_offsets * sizeof(offset_type), pool));

    // Copy valid bits, zero out the bit for the final offset
    // XXX why?
    ARROW_ASSIGN_OR_RAISE(
        auto clean_valid_bits,
        offsets.null_bitmap()->CopySlice(0, BitUtil::BytesForBits(num_offsets - 1)));
    BitUtil::ClearBit(clean_valid_bits->mutable_data(), num_offsets);
    *validity_buf_out = clean_valid_bits;

    const offset_type* raw_offsets = typed_offsets.raw_values();
    auto clean_raw_offsets =
        reinterpret_cast<offset_type*>(clean_offsets->mutable_data());

    // Must work backwards so we can tell how many values were in the last non-null value
    offset_type current_offset = raw_offsets[num_offsets - 1];
    for (int64_t i = num_offsets - 1; i >= 0; --i) {
      if (offsets.IsValid(i)) {
        current_offset = raw_offsets[i];
      }
      clean_raw_offsets[i] = current_offset;
    }

    *offset_buf_out = std::move(clean_offsets);
  } else {
    *validity_buf_out = offsets.null_bitmap();
    *offset_buf_out = typed_offsets.values();
  }

  return Status::OK();
}

template <typename TYPE>
Result<std::shared_ptr<Array>> ListArrayFromArrays(const Array& offsets,
                                                   const Array& values,
                                                   MemoryPool* pool) {
  using offset_type = typename TYPE::offset_type;
  using ArrayType = typename TypeTraits<TYPE>::ArrayType;
  using OffsetArrowType = typename CTypeTraits<offset_type>::ArrowType;

  if (offsets.length() == 0) {
    return Status::Invalid("List offsets must have non-zero length");
  }

  if (offsets.type_id() != OffsetArrowType::type_id) {
    return Status::TypeError("List offsets must be ", OffsetArrowType::type_name());
  }

  std::shared_ptr<Buffer> offset_buf, validity_buf;
  RETURN_NOT_OK(CleanListOffsets<TYPE>(offsets, pool, &offset_buf, &validity_buf));
  BufferVector buffers = {validity_buf, offset_buf};

  auto list_type = std::make_shared<TYPE>(values.type());
  auto internal_data =
      ArrayData::Make(list_type, offsets.length() - 1, std::move(buffers),
                      offsets.null_count(), offsets.offset());
  internal_data->child_data.push_back(values.data());

  return std::make_shared<ArrayType>(internal_data);
}

static std::shared_ptr<Array> SliceArrayWithOffsets(const Array& array, int64_t begin,
                                                    int64_t end) {
  return array.Slice(begin, end - begin);
}

template <typename ListArrayT>
Result<std::shared_ptr<Array>> FlattenListArray(const ListArrayT& list_array,
                                                MemoryPool* memory_pool) {
  const int64_t list_array_length = list_array.length();
  std::shared_ptr<arrow::Array> value_array = list_array.values();

  // Shortcut: if a ListArray does not contain nulls, then simply slice its
  // value array with the first and the last offsets.
  if (list_array.null_count() == 0) {
    return SliceArrayWithOffsets(*value_array, list_array.value_offset(0),
                                 list_array.value_offset(list_array_length));
  }

  // The ListArray contains nulls: there may be a non-empty sub-list behind
  // a null and it must not be contained in the result.
  std::vector<std::shared_ptr<Array>> non_null_fragments;
  int64_t valid_begin = 0;
  while (valid_begin < list_array_length) {
    int64_t valid_end = valid_begin;
    while (valid_end < list_array_length &&
           (list_array.IsValid(valid_end) || list_array.value_length(valid_end) == 0)) {
      ++valid_end;
    }
    if (valid_begin < valid_end) {
      non_null_fragments.push_back(
          SliceArrayWithOffsets(*value_array, list_array.value_offset(valid_begin),
                                list_array.value_offset(valid_end)));
    }
    valid_begin = valid_end + 1;  // skip null entry
  }

  // Final attempt to avoid invoking Concatenate().
  if (non_null_fragments.size() == 1) {
    return non_null_fragments[0];
  }

  std::shared_ptr<Array> flattened;
  RETURN_NOT_OK(Concatenate(non_null_fragments, memory_pool, &flattened));
  return flattened;
}

}  // namespace

ListArray::ListArray(const std::shared_ptr<ArrayData>& data) { SetData(data); }

LargeListArray::LargeListArray(const std::shared_ptr<ArrayData>& data) { SetData(data); }

ListArray::ListArray(const std::shared_ptr<DataType>& type, int64_t length,
                     const std::shared_ptr<Buffer>& value_offsets,
                     const std::shared_ptr<Array>& values,
                     const std::shared_ptr<Buffer>& null_bitmap, int64_t null_count,
                     int64_t offset) {
  ARROW_CHECK_EQ(type->id(), Type::LIST);
  auto internal_data =
      ArrayData::Make(type, length, {null_bitmap, value_offsets}, null_count, offset);
  internal_data->child_data.emplace_back(values->data());
  SetData(internal_data);
}

LargeListArray::LargeListArray(const std::shared_ptr<DataType>& type, int64_t length,
                               const std::shared_ptr<Buffer>& value_offsets,
                               const std::shared_ptr<Array>& values,
                               const std::shared_ptr<Buffer>& null_bitmap,
                               int64_t null_count, int64_t offset) {
  ARROW_CHECK_EQ(type->id(), Type::LARGE_LIST);
  auto internal_data =
      ArrayData::Make(type, length, {null_bitmap, value_offsets}, null_count, offset);
  internal_data->child_data.emplace_back(values->data());
  SetData(internal_data);
}

void ListArray::SetData(const std::shared_ptr<ArrayData>& data,
                        Type::type expected_type_id) {
  this->Array::SetData(data);
  ARROW_CHECK_EQ(data->buffers.size(), 2);
  ARROW_CHECK_EQ(data->type->id(), expected_type_id);
  list_type_ = checked_cast<const ListType*>(data->type.get());

  auto value_offsets = data->buffers[1];
  raw_value_offsets_ = value_offsets == nullptr
                           ? nullptr
                           : reinterpret_cast<const offset_type*>(value_offsets->data());

  ARROW_CHECK_EQ(data_->child_data.size(), 1);
  ARROW_CHECK_EQ(list_type_->value_type()->id(), data->child_data[0]->type->id());
  DCHECK(list_type_->value_type()->Equals(data->child_data[0]->type));
  values_ = MakeArray(data_->child_data[0]);
}

void LargeListArray::SetData(const std::shared_ptr<ArrayData>& data) {
  this->Array::SetData(data);
  ARROW_CHECK_EQ(data->buffers.size(), 2);
  ARROW_CHECK_EQ(data->type->id(), Type::LARGE_LIST);
  list_type_ = checked_cast<const LargeListType*>(data->type.get());

  auto value_offsets = data->buffers[1];
  raw_value_offsets_ = value_offsets == nullptr
                           ? nullptr
                           : reinterpret_cast<const offset_type*>(value_offsets->data());

  ARROW_CHECK_EQ(data_->child_data.size(), 1);
  ARROW_CHECK_EQ(list_type_->value_type()->id(), data->child_data[0]->type->id());
  DCHECK(list_type_->value_type()->Equals(data->child_data[0]->type));
  values_ = MakeArray(data_->child_data[0]);
}

Result<std::shared_ptr<Array>> ListArray::FromArrays(const Array& offsets,
                                                     const Array& values,
                                                     MemoryPool* pool) {
  return ListArrayFromArrays<ListType>(offsets, values, pool);
}

Result<std::shared_ptr<Array>> LargeListArray::FromArrays(const Array& offsets,
                                                          const Array& values,
                                                          MemoryPool* pool) {
  return ListArrayFromArrays<LargeListType>(offsets, values, pool);
}

Status ListArray::FromArrays(const Array& offsets, const Array& values, MemoryPool* pool,
                             std::shared_ptr<Array>* out) {
  return FromArrays(offsets, values, pool).Value(out);
}

Status LargeListArray::FromArrays(const Array& offsets, const Array& values,
                                  MemoryPool* pool, std::shared_ptr<Array>* out) {
  return FromArrays(offsets, values, pool).Value(out);
}

Result<std::shared_ptr<Array>> ListArray::Flatten(MemoryPool* memory_pool) const {
  return FlattenListArray(*this, memory_pool);
}

Result<std::shared_ptr<Array>> LargeListArray::Flatten(MemoryPool* memory_pool) const {
  return FlattenListArray(*this, memory_pool);
}
// ----------------------------------------------------------------------
// MapArray

MapArray::MapArray(const std::shared_ptr<ArrayData>& data) { SetData(data); }

MapArray::MapArray(const std::shared_ptr<DataType>& type, int64_t length,
                   const std::shared_ptr<Buffer>& offsets,
                   const std::shared_ptr<Array>& values,
                   const std::shared_ptr<Buffer>& null_bitmap, int64_t null_count,
                   int64_t offset) {
  SetData(ArrayData::Make(type, length, {null_bitmap, offsets}, {values->data()},
                          null_count, offset));
}

MapArray::MapArray(const std::shared_ptr<DataType>& type, int64_t length,
                   const std::shared_ptr<Buffer>& offsets,
                   const std::shared_ptr<Array>& keys,
                   const std::shared_ptr<Array>& items,
                   const std::shared_ptr<Buffer>& null_bitmap, int64_t null_count,
                   int64_t offset) {
  auto pair_data = ArrayData::Make(type->children()[0]->type(), keys->data()->length,
                                   {nullptr}, {keys->data(), items->data()}, 0, offset);
  auto map_data = ArrayData::Make(type, length, {null_bitmap, offsets}, {pair_data},
                                  null_count, offset);
  SetData(map_data);
}

Result<std::shared_ptr<Array>> MapArray::FromArrays(const std::shared_ptr<Array>& offsets,
                                                    const std::shared_ptr<Array>& keys,
                                                    const std::shared_ptr<Array>& items,
                                                    MemoryPool* pool) {
  using offset_type = typename MapType::offset_type;
  using OffsetArrowType = typename CTypeTraits<offset_type>::ArrowType;

  if (offsets->length() == 0) {
    return Status::Invalid("Map offsets must have non-zero length");
  }

  if (offsets->type_id() != OffsetArrowType::type_id) {
    return Status::TypeError("Map offsets must be ", OffsetArrowType::type_name());
  }

  if (keys->null_count() != 0) {
    return Status::Invalid("Map can not contain NULL valued keys");
  }

  if (keys->length() != items->length()) {
    return Status::Invalid("Map key and item arrays must be equal length");
  }

  std::shared_ptr<Buffer> offset_buf, validity_buf;
  RETURN_NOT_OK(CleanListOffsets<MapType>(*offsets, pool, &offset_buf, &validity_buf));

  auto map_type = std::make_shared<MapType>(keys->type(), items->type());
  return std::make_shared<MapArray>(map_type, offsets->length() - 1, offset_buf, keys,
                                    items, validity_buf, offsets->null_count(),
                                    offsets->offset());
}

Status MapArray::FromArrays(const std::shared_ptr<Array>& offsets,
                            const std::shared_ptr<Array>& keys,
                            const std::shared_ptr<Array>& items, MemoryPool* pool,
                            std::shared_ptr<Array>* out) {
  return FromArrays(offsets, keys, items, pool).Value(out);
}

Status MapArray::ValidateChildData(
    const std::vector<std::shared_ptr<ArrayData>>& child_data) {
  if (child_data.size() != 1) {
    return Status::Invalid("Expected one child array for map array");
  }
  const auto& pair_data = child_data[0];
  if (pair_data->type->id() != Type::STRUCT) {
    return Status::Invalid("Map array child array should have struct type");
  }
  if (pair_data->null_count != 0) {
    return Status::Invalid("Map array child array should have no nulls");
  }
  if (pair_data->child_data.size() != 2) {
    return Status::Invalid("Map array child array should have two fields");
  }
  if (pair_data->child_data[0]->null_count != 0) {
    return Status::Invalid("Map array keys array should have no nulls");
  }
  return Status::OK();
}

void MapArray::SetData(const std::shared_ptr<ArrayData>& data) {
  ARROW_CHECK_OK(ValidateChildData(data->child_data));

  this->ListArray::SetData(data, Type::MAP);
  map_type_ = checked_cast<const MapType*>(data->type.get());
  const auto& pair_data = data->child_data[0];
  keys_ = MakeArray(pair_data->child_data[0]);
  items_ = MakeArray(pair_data->child_data[1]);
}

// ----------------------------------------------------------------------
// FixedSizeListArray

FixedSizeListArray::FixedSizeListArray(const std::shared_ptr<ArrayData>& data) {
  SetData(data);
}

FixedSizeListArray::FixedSizeListArray(const std::shared_ptr<DataType>& type,
                                       int64_t length,
                                       const std::shared_ptr<Array>& values,
                                       const std::shared_ptr<Buffer>& null_bitmap,
                                       int64_t null_count, int64_t offset) {
  auto internal_data = ArrayData::Make(type, length, {null_bitmap}, null_count, offset);
  internal_data->child_data.emplace_back(values->data());
  SetData(internal_data);
}

void FixedSizeListArray::SetData(const std::shared_ptr<ArrayData>& data) {
  ARROW_CHECK_EQ(data->type->id(), Type::FIXED_SIZE_LIST);
  this->Array::SetData(data);

  ARROW_CHECK_EQ(list_type()->value_type()->id(), data->child_data[0]->type->id());
  DCHECK(list_type()->value_type()->Equals(data->child_data[0]->type));
  list_size_ = list_type()->list_size();

  ARROW_CHECK_EQ(data_->child_data.size(), 1);
  values_ = MakeArray(data_->child_data[0]);
}

const FixedSizeListType* FixedSizeListArray::list_type() const {
  return checked_cast<const FixedSizeListType*>(data_->type.get());
}

std::shared_ptr<DataType> FixedSizeListArray::value_type() const {
  return list_type()->value_type();
}

std::shared_ptr<Array> FixedSizeListArray::values() const { return values_; }

Result<std::shared_ptr<Array>> FixedSizeListArray::FromArrays(
    const std::shared_ptr<Array>& values, int32_t list_size) {
  if (list_size <= 0) {
    return Status::Invalid("list_size needs to be a strict positive integer");
  }

  if ((values->length() % list_size) != 0) {
    return Status::Invalid(
        "The length of the values Array needs to be a multiple of the list_size");
  }
  int64_t length = values->length() / list_size;
  auto list_type = std::make_shared<FixedSizeListType>(values->type(), list_size);
  std::shared_ptr<Buffer> validity_buf;

  return std::make_shared<FixedSizeListArray>(list_type, length, values, validity_buf,
                                              /*null_count=*/0, /*offset=*/0);
}

// ----------------------------------------------------------------------
// String and binary

BinaryArray::BinaryArray(const std::shared_ptr<ArrayData>& data) {
  ARROW_CHECK_EQ(data->type->id(), Type::BINARY);
  SetData(data);
}

BinaryArray::BinaryArray(int64_t length, const std::shared_ptr<Buffer>& value_offsets,
                         const std::shared_ptr<Buffer>& data,
                         const std::shared_ptr<Buffer>& null_bitmap, int64_t null_count,
                         int64_t offset) {
  SetData(ArrayData::Make(binary(), length, {null_bitmap, value_offsets, data},
                          null_count, offset));
}

LargeBinaryArray::LargeBinaryArray(const std::shared_ptr<ArrayData>& data) {
  ARROW_CHECK_EQ(data->type->id(), Type::LARGE_BINARY);
  SetData(data);
}

LargeBinaryArray::LargeBinaryArray(int64_t length,
                                   const std::shared_ptr<Buffer>& value_offsets,
                                   const std::shared_ptr<Buffer>& data,
                                   const std::shared_ptr<Buffer>& null_bitmap,
                                   int64_t null_count, int64_t offset) {
  SetData(ArrayData::Make(large_binary(), length, {null_bitmap, value_offsets, data},
                          null_count, offset));
}

StringArray::StringArray(const std::shared_ptr<ArrayData>& data) {
  ARROW_CHECK_EQ(data->type->id(), Type::STRING);
  SetData(data);
}

StringArray::StringArray(int64_t length, const std::shared_ptr<Buffer>& value_offsets,
                         const std::shared_ptr<Buffer>& data,
                         const std::shared_ptr<Buffer>& null_bitmap, int64_t null_count,
                         int64_t offset) {
  SetData(ArrayData::Make(utf8(), length, {null_bitmap, value_offsets, data}, null_count,
                          offset));
}

LargeStringArray::LargeStringArray(const std::shared_ptr<ArrayData>& data) {
  ARROW_CHECK_EQ(data->type->id(), Type::LARGE_STRING);
  SetData(data);
}

LargeStringArray::LargeStringArray(int64_t length,
                                   const std::shared_ptr<Buffer>& value_offsets,
                                   const std::shared_ptr<Buffer>& data,
                                   const std::shared_ptr<Buffer>& null_bitmap,
                                   int64_t null_count, int64_t offset) {
  SetData(ArrayData::Make(large_utf8(), length, {null_bitmap, value_offsets, data},
                          null_count, offset));
}

// ----------------------------------------------------------------------
// Fixed width binary

FixedSizeBinaryArray::FixedSizeBinaryArray(const std::shared_ptr<ArrayData>& data) {
  SetData(data);
}

FixedSizeBinaryArray::FixedSizeBinaryArray(const std::shared_ptr<DataType>& type,
                                           int64_t length,
                                           const std::shared_ptr<Buffer>& data,
                                           const std::shared_ptr<Buffer>& null_bitmap,
                                           int64_t null_count, int64_t offset)
    : PrimitiveArray(type, length, data, null_bitmap, null_count, offset),
      byte_width_(checked_cast<const FixedSizeBinaryType&>(*type).byte_width()) {}

const uint8_t* FixedSizeBinaryArray::GetValue(int64_t i) const {
  return raw_values_ + (i + data_->offset) * byte_width_;
}

// ----------------------------------------------------------------------
// Day time interval

DayTimeIntervalArray::DayTimeIntervalArray(const std::shared_ptr<ArrayData>& data) {
  SetData(data);
}

DayTimeIntervalArray::DayTimeIntervalArray(const std::shared_ptr<DataType>& type,
                                           int64_t length,
                                           const std::shared_ptr<Buffer>& data,
                                           const std::shared_ptr<Buffer>& null_bitmap,
                                           int64_t null_count, int64_t offset)
    : PrimitiveArray(type, length, data, null_bitmap, null_count, offset) {}

DayTimeIntervalType::DayMilliseconds DayTimeIntervalArray::GetValue(int64_t i) const {
  DCHECK(i < length());
  return *reinterpret_cast<const DayTimeIntervalType::DayMilliseconds*>(
      raw_values_ + (i + data_->offset) * byte_width());
}

// ----------------------------------------------------------------------
// Decimal

Decimal128Array::Decimal128Array(const std::shared_ptr<ArrayData>& data)
    : FixedSizeBinaryArray(data) {
  ARROW_CHECK_EQ(data->type->id(), Type::DECIMAL);
}

std::string Decimal128Array::FormatValue(int64_t i) const {
  const auto& type_ = checked_cast<const Decimal128Type&>(*type());
  const Decimal128 value(GetValue(i));
  return value.ToString(type_.scale());
}

// ----------------------------------------------------------------------
// Struct

StructArray::StructArray(const std::shared_ptr<ArrayData>& data) {
  ARROW_CHECK_EQ(data->type->id(), Type::STRUCT);
  SetData(data);
  boxed_fields_.resize(data->child_data.size());
}

StructArray::StructArray(const std::shared_ptr<DataType>& type, int64_t length,
                         const std::vector<std::shared_ptr<Array>>& children,
                         std::shared_ptr<Buffer> null_bitmap, int64_t null_count,
                         int64_t offset) {
  ARROW_CHECK_EQ(type->id(), Type::STRUCT);
  SetData(ArrayData::Make(type, length, {null_bitmap}, null_count, offset));
  for (const auto& child : children) {
    data_->child_data.push_back(child->data());
  }
  boxed_fields_.resize(children.size());
}

Result<std::shared_ptr<StructArray>> StructArray::Make(
    const std::vector<std::shared_ptr<Array>>& children,
    const std::vector<std::shared_ptr<Field>>& fields,
    std::shared_ptr<Buffer> null_bitmap, int64_t null_count, int64_t offset) {
  if (children.size() != fields.size()) {
    return Status::Invalid("Mismatching number of fields and child arrays");
  }
  int64_t length = 0;
  if (children.size() == 0) {
    return Status::Invalid("Can't infer struct array length with 0 child arrays");
  }
  length = children.front()->length();
  for (const auto& child : children) {
    if (length != child->length()) {
      return Status::Invalid("Mismatching child array lengths");
    }
  }
  if (offset > length) {
    return Status::IndexError("Offset greater than length of child arrays");
  }
  if (null_bitmap == nullptr) {
    if (null_count > 0) {
      return Status::Invalid("null_count = ", null_count, " but no null bitmap given");
    }
    null_count = 0;
  }
  return std::make_shared<StructArray>(struct_(fields), length - offset, children,
                                       null_bitmap, null_count, offset);
}

Result<std::shared_ptr<StructArray>> StructArray::Make(
    const std::vector<std::shared_ptr<Array>>& children,
    const std::vector<std::string>& field_names, std::shared_ptr<Buffer> null_bitmap,
    int64_t null_count, int64_t offset) {
  if (children.size() != field_names.size()) {
    return Status::Invalid("Mismatching number of field names and child arrays");
  }
  std::vector<std::shared_ptr<Field>> fields(children.size());
  for (size_t i = 0; i < children.size(); ++i) {
    fields[i] = ::arrow::field(field_names[i], children[i]->type());
  }
  return Make(children, fields, std::move(null_bitmap), null_count, offset);
}

const StructType* StructArray::struct_type() const {
  return checked_cast<const StructType*>(data_->type.get());
}

std::shared_ptr<Array> StructArray::field(int i) const {
  std::shared_ptr<Array> result = internal::atomic_load(&boxed_fields_[i]);
  if (!result) {
    std::shared_ptr<ArrayData> field_data;
    if (data_->offset != 0 || data_->child_data[i]->length != data_->length) {
      field_data = std::make_shared<ArrayData>(
          data_->child_data[i]->Slice(data_->offset, data_->length));
    } else {
      field_data = data_->child_data[i];
    }
    result = MakeArray(field_data);
    internal::atomic_store(&boxed_fields_[i], result);
  }
  return result;
}

std::shared_ptr<Array> StructArray::GetFieldByName(const std::string& name) const {
  int i = struct_type()->GetFieldIndex(name);
  return i == -1 ? nullptr : field(i);
}

Result<ArrayVector> StructArray::Flatten(MemoryPool* pool) const {
  ArrayVector flattened;
  flattened.reserve(data_->child_data.size());
  std::shared_ptr<Buffer> null_bitmap = data_->buffers[0];

  for (const auto& child_data_ptr : data_->child_data) {
    auto child_data = child_data_ptr->Copy();

    std::shared_ptr<Buffer> flattened_null_bitmap;
    int64_t flattened_null_count = kUnknownNullCount;

    // Need to adjust for parent offset
    if (data_->offset != 0 || data_->length != child_data->length) {
      *child_data = child_data->Slice(data_->offset, data_->length);
    }
    std::shared_ptr<Buffer> child_null_bitmap = child_data->buffers[0];
    const int64_t child_offset = child_data->offset;

    // The validity of a flattened datum is the logical AND of the struct
    // element's validity and the individual field element's validity.
    if (null_bitmap && child_null_bitmap) {
      ARROW_ASSIGN_OR_RAISE(
          flattened_null_bitmap,
          BitmapAnd(pool, child_null_bitmap->data(), child_offset, null_bitmap_data_,
                    data_->offset, data_->length, child_offset));
    } else if (child_null_bitmap) {
      flattened_null_bitmap = child_null_bitmap;
      flattened_null_count = child_data->null_count;
    } else if (null_bitmap) {
      if (child_offset == data_->offset) {
        flattened_null_bitmap = null_bitmap;
      } else {
        ARROW_ASSIGN_OR_RAISE(
            flattened_null_bitmap,
            CopyBitmap(pool, null_bitmap_data_, data_->offset, data_->length));
      }
      flattened_null_count = data_->null_count;
    } else {
      flattened_null_count = 0;
    }

    auto flattened_data = child_data->Copy();
    flattened_data->buffers[0] = flattened_null_bitmap;
    flattened_data->null_count = flattened_null_count;

    flattened.push_back(MakeArray(flattened_data));
  }

  return flattened;
}

Status StructArray::Flatten(MemoryPool* pool, ArrayVector* out) const {
  return Flatten(pool).Value(out);
}

// ----------------------------------------------------------------------
// UnionArray

void UnionArray::SetData(const std::shared_ptr<ArrayData>& data) {
  this->Array::SetData(data);

  ARROW_CHECK_EQ(data->type->id(), Type::UNION);
  ARROW_CHECK_EQ(data->buffers.size(), 3);
  union_type_ = checked_cast<const UnionType*>(data_->type.get());

  auto type_codes = data_->buffers[1];
  auto value_offsets = data_->buffers[2];
  raw_type_codes_ = type_codes == nullptr
                        ? nullptr
                        : reinterpret_cast<const int8_t*>(type_codes->data());
  raw_value_offsets_ = value_offsets == nullptr
                           ? nullptr
                           : reinterpret_cast<const int32_t*>(value_offsets->data());
  boxed_fields_.resize(data->child_data.size());
}

UnionArray::UnionArray(const std::shared_ptr<ArrayData>& data) { SetData(data); }

UnionArray::UnionArray(const std::shared_ptr<DataType>& type, int64_t length,
                       const std::vector<std::shared_ptr<Array>>& children,
                       const std::shared_ptr<Buffer>& type_codes,
                       const std::shared_ptr<Buffer>& value_offsets,
                       const std::shared_ptr<Buffer>& null_bitmap, int64_t null_count,
                       int64_t offset) {
  auto internal_data = ArrayData::Make(
      type, length, {null_bitmap, type_codes, value_offsets}, null_count, offset);
  for (const auto& child : children) {
    internal_data->child_data.push_back(child->data());
  }
  SetData(internal_data);
}

Result<std::shared_ptr<Array>> UnionArray::MakeDense(
    const Array& type_ids, const Array& value_offsets,
    const std::vector<std::shared_ptr<Array>>& children,
    const std::vector<std::string>& field_names, const std::vector<int8_t>& type_codes) {
  if (value_offsets.length() == 0) {
    return Status::Invalid("UnionArray offsets must have non-zero length");
  }

  if (value_offsets.type_id() != Type::INT32) {
    return Status::TypeError("UnionArray offsets must be signed int32");
  }

  if (type_ids.type_id() != Type::INT8) {
    return Status::TypeError("UnionArray type_ids must be signed int8");
  }

  if (value_offsets.null_count() != 0) {
    return Status::Invalid("MakeDense does not allow NAs in value_offsets");
  }

  if (field_names.size() > 0 && field_names.size() != children.size()) {
    return Status::Invalid("field_names must have the same length as children");
  }

  if (type_codes.size() > 0 && type_codes.size() != children.size()) {
    return Status::Invalid("type_codes must have the same length as children");
  }

  BufferVector buffers = {type_ids.null_bitmap(),
                          checked_cast<const Int8Array&>(type_ids).values(),
                          checked_cast<const Int32Array&>(value_offsets).values()};

  std::shared_ptr<DataType> union_type =
      union_(children, field_names, type_codes, UnionMode::DENSE);
  auto internal_data = ArrayData::Make(union_type, type_ids.length(), std::move(buffers),
                                       type_ids.null_count(), type_ids.offset());
  for (const auto& child : children) {
    internal_data->child_data.push_back(child->data());
  }
  return std::make_shared<UnionArray>(internal_data);
}

Result<std::shared_ptr<Array>> UnionArray::MakeSparse(
    const Array& type_ids, const std::vector<std::shared_ptr<Array>>& children,
    const std::vector<std::string>& field_names, const std::vector<int8_t>& type_codes) {
  if (type_ids.type_id() != Type::INT8) {
    return Status::TypeError("UnionArray type_ids must be signed int8");
  }

  if (field_names.size() > 0 && field_names.size() != children.size()) {
    return Status::Invalid("field_names must have the same length as children");
  }

  if (type_codes.size() > 0 && type_codes.size() != children.size()) {
    return Status::Invalid("type_codes must have the same length as children");
  }

  BufferVector buffers = {type_ids.null_bitmap(),
                          checked_cast<const Int8Array&>(type_ids).values(), nullptr};
  std::shared_ptr<DataType> union_type =
      union_(children, field_names, type_codes, UnionMode::SPARSE);
  auto internal_data = ArrayData::Make(union_type, type_ids.length(), std::move(buffers),
                                       type_ids.null_count(), type_ids.offset());
  for (const auto& child : children) {
    internal_data->child_data.push_back(child->data());
    if (child->length() != type_ids.length()) {
      return Status::Invalid(
          "Sparse UnionArray must have len(child) == len(type_ids) for all children");
    }
  }
  return std::make_shared<UnionArray>(internal_data);
}

std::shared_ptr<Array> UnionArray::child(int i) const {
  std::shared_ptr<Array> result = internal::atomic_load(&boxed_fields_[i]);
  if (!result) {
    std::shared_ptr<ArrayData> child_data = data_->child_data[i]->Copy();
    if (mode() == UnionMode::SPARSE) {
      // Sparse union: need to adjust child if union is sliced
      // (for dense unions, the need to lookup through the offsets
      //  makes this unnecessary)
      if (data_->offset != 0 || child_data->length > data_->length) {
        *child_data = child_data->Slice(data_->offset, data_->length);
      }
    }
    result = MakeArray(child_data);
    internal::atomic_store(&boxed_fields_[i], result);
  }
  return result;
}

// ----------------------------------------------------------------------
// DictionaryArray

/// \brief Perform validation check to determine if all dictionary indices
/// are within valid range (0 <= index < upper_bound)
///
/// \param[in] indices array of dictionary indices
/// \param[in] upper_bound upper bound of valid range for indices
/// \return Status
template <typename ArrowType>
Status ValidateDictionaryIndices(const std::shared_ptr<Array>& indices,
                                 const int64_t upper_bound) {
  using ArrayType = typename TypeTraits<ArrowType>::ArrayType;
  const auto& array = checked_cast<const ArrayType&>(*indices);
  const typename ArrowType::c_type* data = array.raw_values();
  const int64_t size = array.length();

  if (array.null_count() == 0) {
    for (int64_t idx = 0; idx < size; ++idx) {
      if (data[idx] < 0 || data[idx] >= upper_bound) {
        return Status::Invalid("Dictionary has out-of-bound index [0, dict.length)");
      }
    }
  } else {
    for (int64_t idx = 0; idx < size; ++idx) {
      if (!array.IsNull(idx)) {
        if (data[idx] < 0 || data[idx] >= upper_bound) {
          return Status::Invalid("Dictionary has out-of-bound index [0, dict.length)");
        }
      }
    }
  }

  return Status::OK();
}

std::shared_ptr<Array> DictionaryArray::indices() const { return indices_; }

DictionaryArray::DictionaryArray(const std::shared_ptr<ArrayData>& data)
    : dict_type_(checked_cast<const DictionaryType*>(data->type.get())) {
  ARROW_CHECK_EQ(data->type->id(), Type::DICTIONARY);
  ARROW_CHECK_NE(data->dictionary, nullptr);
  SetData(data);
}

void DictionaryArray::SetData(const std::shared_ptr<ArrayData>& data) {
  this->Array::SetData(data);
  auto indices_data = data_->Copy();
  indices_data->type = dict_type_->index_type();
  indices_data->dictionary = nullptr;
  indices_ = MakeArray(indices_data);
}

DictionaryArray::DictionaryArray(const std::shared_ptr<DataType>& type,
                                 const std::shared_ptr<Array>& indices,
                                 const std::shared_ptr<Array>& dictionary)
    : dict_type_(checked_cast<const DictionaryType*>(type.get())) {
  ARROW_CHECK_EQ(type->id(), Type::DICTIONARY);
  ARROW_CHECK_EQ(indices->type_id(), dict_type_->index_type()->id());
  ARROW_CHECK_EQ(dict_type_->value_type()->id(), dictionary->type()->id());
  DCHECK(dict_type_->value_type()->Equals(*dictionary->type()));
  auto data = indices->data()->Copy();
  data->type = type;
  data->dictionary = dictionary;
  SetData(data);
}

std::shared_ptr<Array> DictionaryArray::dictionary() const { return data_->dictionary; }

Result<std::shared_ptr<Array>> DictionaryArray::FromArrays(
    const std::shared_ptr<DataType>& type, const std::shared_ptr<Array>& indices,
    const std::shared_ptr<Array>& dictionary) {
  if (type->id() != Type::DICTIONARY) {
    return Status::TypeError("Expected a dictionary type");
  }
  const auto& dict = checked_cast<const DictionaryType&>(*type);
  ARROW_CHECK_EQ(indices->type_id(), dict.index_type()->id());

  int64_t upper_bound = dictionary->length();
  Status is_valid;

  switch (indices->type_id()) {
    case Type::INT8:
      is_valid = ValidateDictionaryIndices<Int8Type>(indices, upper_bound);
      break;
    case Type::INT16:
      is_valid = ValidateDictionaryIndices<Int16Type>(indices, upper_bound);
      break;
    case Type::INT32:
      is_valid = ValidateDictionaryIndices<Int32Type>(indices, upper_bound);
      break;
    case Type::INT64:
      is_valid = ValidateDictionaryIndices<Int64Type>(indices, upper_bound);
      break;
    default:
      return Status::NotImplemented("Dictionary index type not supported: ",
                                    indices->type()->ToString());
  }
  RETURN_NOT_OK(is_valid);
  return std::make_shared<DictionaryArray>(type, indices, dictionary);
}

Status DictionaryArray::FromArrays(const std::shared_ptr<DataType>& type,
                                   const std::shared_ptr<Array>& indices,
                                   const std::shared_ptr<Array>& dictionary,
                                   std::shared_ptr<Array>* out) {
  return FromArrays(type, indices, dictionary).Value(out);
}

bool DictionaryArray::CanCompareIndices(const DictionaryArray& other) const {
  DCHECK(dictionary()->type()->Equals(other.dictionary()->type()))
      << "dictionaries have differing type " << *dictionary()->type() << " vs "
      << *other.dictionary()->type();

  if (!indices()->type()->Equals(other.indices()->type())) {
    return false;
  }

  auto min_length = std::min(dictionary()->length(), other.dictionary()->length());
  return dictionary()->RangeEquals(other.dictionary(), 0, min_length, 0);
}

// ----------------------------------------------------------------------
// Implement Array::View

namespace {

void AccumulateLayouts(const std::shared_ptr<DataType>& type,
                       std::vector<DataTypeLayout>* layouts) {
  layouts->push_back(type->layout());
  for (const auto& child : type->children()) {
    AccumulateLayouts(child->type(), layouts);
  }
}

void AccumulateArrayData(const std::shared_ptr<ArrayData>& data,
                         std::vector<std::shared_ptr<ArrayData>>* out) {
  out->push_back(data);
  for (const auto& child : data->child_data) {
    AccumulateArrayData(child, out);
  }
}

struct ViewDataImpl {
  std::shared_ptr<DataType> root_in_type;
  std::shared_ptr<DataType> root_out_type;
  std::vector<DataTypeLayout> in_layouts;
  std::vector<std::shared_ptr<ArrayData>> in_data;
  int64_t in_data_length;
  size_t in_layout_idx = 0;
  size_t in_buffer_idx = 0;
  bool input_exhausted = false;

  Status InvalidView(const std::string& msg) {
    return Status::Invalid("Can't view array of type ", root_in_type->ToString(), " as ",
                           root_out_type->ToString(), ": ", msg);
  }

  void AdjustInputPointer() {
    if (input_exhausted) {
      return;
    }
    while (true) {
      // Skip exhausted layout (might be empty layout)
      while (in_buffer_idx >= in_layouts[in_layout_idx].buffers.size()) {
        in_buffer_idx = 0;
        ++in_layout_idx;
        if (in_layout_idx >= in_layouts.size()) {
          input_exhausted = true;
          return;
        }
      }
      const auto& in_spec = in_layouts[in_layout_idx].buffers[in_buffer_idx];
      if (in_spec.kind != DataTypeLayout::ALWAYS_NULL) {
        return;
      }
      // Skip always-null input buffers
      // (e.g. buffer 0 of a null type or buffer 2 of a sparse union)
      ++in_buffer_idx;
    }
  }

  Status CheckInputAvailable() {
    if (input_exhausted) {
      return InvalidView("not enough buffers for view type");
    }
    return Status::OK();
  }

  Status CheckInputExhausted() {
    if (!input_exhausted) {
      return InvalidView("too many buffers for view type");
    }
    return Status::OK();
  }

  Result<std::shared_ptr<Array>> GetDictionaryView(const DataType& out_type) {
    if (in_data[in_layout_idx]->type->id() != Type::DICTIONARY) {
      return InvalidView("Cannot get view as dictionary type");
    }
    const auto& dict_out_type = static_cast<const DictionaryType&>(out_type);
    return in_data[in_layout_idx]->dictionary->View(dict_out_type.value_type());
  }

  Status MakeDataView(const std::shared_ptr<Field>& out_field,
                      std::shared_ptr<ArrayData>* out) {
    const auto out_type = out_field->type();
    const auto out_layout = out_type->layout();

    AdjustInputPointer();
    int64_t out_length = in_data_length;
    int64_t out_offset = 0;
    int64_t out_null_count;

    std::shared_ptr<Array> dictionary;
    if (out_type->id() == Type::DICTIONARY) {
      ARROW_ASSIGN_OR_RAISE(dictionary, GetDictionaryView(*out_type));
    }

    // No type has a purely empty layout
    DCHECK_GT(out_layout.buffers.size(), 0);

    if (out_layout.buffers[0].kind == DataTypeLayout::ALWAYS_NULL) {
      // Assuming null type or equivalent.
      DCHECK_EQ(out_layout.buffers.size(), 1);
      *out = ArrayData::Make(out_type, out_length, {nullptr}, out_length);
      return Status::OK();
    }

    std::vector<std::shared_ptr<Buffer>> out_buffers;

    // Process null bitmap
    DCHECK_EQ(out_layout.buffers[0].kind, DataTypeLayout::BITMAP);
    if (in_buffer_idx == 0) {
      // Copy input null bitmap
      RETURN_NOT_OK(CheckInputAvailable());
      const auto& in_data_item = in_data[in_layout_idx];
      if (!out_field->nullable() && in_data_item->GetNullCount() != 0) {
        return InvalidView("nulls in input cannot be viewed as non-nullable");
      }
      DCHECK_GT(in_data_item->buffers.size(), in_buffer_idx);
      out_buffers.push_back(in_data_item->buffers[in_buffer_idx]);
      out_length = in_data_item->length;
      out_offset = in_data_item->offset;
      out_null_count = in_data_item->null_count;
      ++in_buffer_idx;
      AdjustInputPointer();
    } else {
      // No null bitmap in input, append no-nulls bitmap
      out_buffers.push_back(nullptr);
      out_null_count = 0;
    }

    // Process other buffers in output layout
    for (size_t out_buffer_idx = 1; out_buffer_idx < out_layout.buffers.size();
         ++out_buffer_idx) {
      const auto& out_spec = out_layout.buffers[out_buffer_idx];
      // If always-null buffer is expected, just construct it
      if (out_spec.kind == DataTypeLayout::ALWAYS_NULL) {
        out_buffers.push_back(nullptr);
        continue;
      }

      // If input buffer is null bitmap, try to ignore it
      while (in_buffer_idx == 0) {
        RETURN_NOT_OK(CheckInputAvailable());
        if (in_data[in_layout_idx]->GetNullCount() != 0) {
          return InvalidView("cannot represent nested nulls");
        }
        ++in_buffer_idx;
        AdjustInputPointer();
      }

      RETURN_NOT_OK(CheckInputAvailable());
      const auto& in_spec = in_layouts[in_layout_idx].buffers[in_buffer_idx];
      if (out_spec != in_spec) {
        return InvalidView("incompatible layouts");
      }
      // Copy input buffer
      const auto& in_data_item = in_data[in_layout_idx];
      out_length = in_data_item->length;
      out_offset = in_data_item->offset;
      DCHECK_GT(in_data_item->buffers.size(), in_buffer_idx);
      out_buffers.push_back(in_data_item->buffers[in_buffer_idx]);
      ++in_buffer_idx;
      AdjustInputPointer();
    }

    std::shared_ptr<ArrayData> out_data = ArrayData::Make(
        out_type, out_length, std::move(out_buffers), out_null_count, out_offset);
    out_data->dictionary = dictionary;

    // Process children recursively, depth-first
    for (const auto& child_field : out_type->children()) {
      std::shared_ptr<ArrayData> child_data;
      RETURN_NOT_OK(MakeDataView(child_field, &child_data));
      out_data->child_data.push_back(std::move(child_data));
    }
    *out = std::move(out_data);
    return Status::OK();
  }
};

}  // namespace

Result<std::shared_ptr<Array>> Array::View(
    const std::shared_ptr<DataType>& out_type) const {
  ViewDataImpl impl;
  impl.root_in_type = data_->type;
  impl.root_out_type = out_type;
  AccumulateLayouts(impl.root_in_type, &impl.in_layouts);
  AccumulateArrayData(data_, &impl.in_data);
  impl.in_data_length = data_->length;

  std::shared_ptr<ArrayData> out_data;
  // Dummy field for output type
  auto out_field = field("", out_type);
  RETURN_NOT_OK(impl.MakeDataView(out_field, &out_data));
  RETURN_NOT_OK(impl.CheckInputExhausted());
  return MakeArray(out_data);
}

Status Array::View(const std::shared_ptr<DataType>& out_type,
                   std::shared_ptr<Array>* out) const {
  return View(out_type).Value(out);
}

// ----------------------------------------------------------------------
// Implement Array::Accept as inline visitor

Status Array::Accept(ArrayVisitor* visitor) const {
  return VisitArrayInline(*this, visitor);
}

Status Array::Validate() const { return internal::ValidateArray(*this); }

Status Array::ValidateFull() const {
  RETURN_NOT_OK(internal::ValidateArray(*this));
  return internal::ValidateArrayData(*this);
}

// ----------------------------------------------------------------------
// Loading from ArrayData

namespace internal {

class ArrayDataWrapper {
 public:
  ArrayDataWrapper(const std::shared_ptr<ArrayData>& data, std::shared_ptr<Array>* out)
      : data_(data), out_(out) {}

  template <typename T>
  Status Visit(const T&) {
    using ArrayType = typename TypeTraits<T>::ArrayType;
    *out_ = std::make_shared<ArrayType>(data_);
    return Status::OK();
  }

  Status Visit(const ExtensionType& type) {
    *out_ = type.MakeArray(data_);
    return Status::OK();
  }

  const std::shared_ptr<ArrayData>& data_;
  std::shared_ptr<Array>* out_;
};

}  // namespace internal

std::shared_ptr<Array> MakeArray(const std::shared_ptr<ArrayData>& data) {
  std::shared_ptr<Array> out;
  internal::ArrayDataWrapper wrapper_visitor(data, &out);
  DCHECK_OK(VisitTypeInline(*data->type, &wrapper_visitor));
  DCHECK(out);
  return out;
}

// ----------------------------------------------------------------------
// Misc APIs

namespace internal {

// get the maximum buffer length required, then allocate a single zeroed buffer
// to use anywhere a buffer is required
class NullArrayFactory {
 public:
  struct GetBufferLength {
    GetBufferLength(const std::shared_ptr<DataType>& type, int64_t length)
        : type_(*type), length_(length), buffer_length_(BitUtil::BytesForBits(length)) {}

    Result<int64_t> Finish() && {
      RETURN_NOT_OK(VisitTypeInline(type_, this));
      return buffer_length_;
    }

    template <typename T, typename = decltype(TypeTraits<T>::bytes_required(0))>
    Status Visit(const T&) {
      return MaxOf(TypeTraits<T>::bytes_required(length_));
    }

    template <typename T>
    enable_if_var_size_list<T, Status> Visit(const T&) {
      // values array may be empty, but there must be at least one offset of 0
      return MaxOf(sizeof(typename T::offset_type) * (length_ + 1));
    }

    template <typename T>
    enable_if_base_binary<T, Status> Visit(const T&) {
      // values buffer may be empty, but there must be at least one offset of 0
      return MaxOf(sizeof(typename T::offset_type) * (length_ + 1));
    }

    Status Visit(const FixedSizeListType& type) {
      return MaxOf(GetBufferLength(type.value_type(), type.list_size() * length_));
    }

    Status Visit(const FixedSizeBinaryType& type) {
      return MaxOf(type.byte_width() * length_);
    }

    Status Visit(const StructType& type) {
      for (const auto& child : type.children()) {
        RETURN_NOT_OK(MaxOf(GetBufferLength(child->type(), length_)));
      }
      return Status::OK();
    }

    Status Visit(const UnionType& type) {
      // type codes
      RETURN_NOT_OK(MaxOf(length_));
      if (type.mode() == UnionMode::DENSE) {
        // offsets
        RETURN_NOT_OK(MaxOf(sizeof(int32_t) * length_));
      }
      for (const auto& child : type.children()) {
        RETURN_NOT_OK(MaxOf(GetBufferLength(child->type(), length_)));
      }
      return Status::OK();
    }

    Status Visit(const DictionaryType& type) {
      RETURN_NOT_OK(MaxOf(GetBufferLength(type.value_type(), length_)));
      return MaxOf(GetBufferLength(type.index_type(), length_));
    }

    Status Visit(const ExtensionType& type) {
      // XXX is an extension array's length always == storage length
      return MaxOf(GetBufferLength(type.storage_type(), length_));
    }

    Status Visit(const DataType& type) {
      return Status::NotImplemented("construction of all-null ", type);
    }

   private:
    Status MaxOf(GetBufferLength&& other) {
      ARROW_ASSIGN_OR_RAISE(int64_t buffer_length, std::move(other).Finish());
      return MaxOf(buffer_length);
    }

    Status MaxOf(int64_t buffer_length) {
      if (buffer_length > buffer_length_) {
        buffer_length_ = buffer_length;
      }
      return Status::OK();
    }

    const DataType& type_;
    int64_t length_, buffer_length_;
  };

  NullArrayFactory(MemoryPool* pool, const std::shared_ptr<DataType>& type,
                   int64_t length)
      : pool_(pool), type_(type), length_(length) {}

  Status CreateBuffer() {
    ARROW_ASSIGN_OR_RAISE(int64_t buffer_length,
                          GetBufferLength(type_, length_).Finish());
    ARROW_ASSIGN_OR_RAISE(buffer_, AllocateBuffer(buffer_length, pool_));
    std::memset(buffer_->mutable_data(), 0, buffer_->size());
    return Status::OK();
  }

  Result<std::shared_ptr<ArrayData>> Create() {
    if (buffer_ == nullptr) {
      RETURN_NOT_OK(CreateBuffer());
    }
    std::vector<std::shared_ptr<ArrayData>> child_data(type_->num_children());
    out_ = ArrayData::Make(type_, length_, {buffer_}, child_data, length_, 0);
    RETURN_NOT_OK(VisitTypeInline(*type_, this));
    return out_;
  }

  Status Visit(const NullType&) { return Status::OK(); }

  Status Visit(const FixedWidthType&) {
    out_->buffers.resize(2, buffer_);
    return Status::OK();
  }

  template <typename T>
  enable_if_base_binary<T, Status> Visit(const T&) {
    out_->buffers.resize(3, buffer_);
    return Status::OK();
  }

  template <typename T>
  enable_if_var_size_list<T, Status> Visit(const T& type) {
    out_->buffers.resize(2, buffer_);
    ARROW_ASSIGN_OR_RAISE(out_->child_data[0], CreateChild(0, length_));
    return Status::OK();
  }

  Status Visit(const FixedSizeListType& type) {
    ARROW_ASSIGN_OR_RAISE(out_->child_data[0],
                          CreateChild(0, length_ * type.list_size()));
    return Status::OK();
  }

  Status Visit(const StructType& type) {
    for (int i = 0; i < type_->num_children(); ++i) {
      ARROW_ASSIGN_OR_RAISE(out_->child_data[i], CreateChild(i, length_));
    }
    return Status::OK();
  }

  Status Visit(const UnionType& type) {
    if (type.mode() == UnionMode::DENSE) {
      out_->buffers.resize(3, buffer_);
    } else {
      out_->buffers = {buffer_, buffer_, nullptr};
    }

    for (int i = 0; i < type_->num_children(); ++i) {
      ARROW_ASSIGN_OR_RAISE(out_->child_data[i], CreateChild(i, length_));
    }
    return Status::OK();
  }

  Status Visit(const DictionaryType& type) {
    out_->buffers.resize(2, buffer_);
    ARROW_ASSIGN_OR_RAISE(out_->dictionary, MakeArrayOfNull(type.value_type(), 0));
    return Status::OK();
  }

  Status Visit(const DataType& type) {
    return Status::NotImplemented("construction of all-null ", type);
  }

  Result<std::shared_ptr<ArrayData>> CreateChild(int i, int64_t length) {
    NullArrayFactory child_factory(pool_, type_->child(i)->type(), length);
    child_factory.buffer_ = buffer_;
    return child_factory.Create();
  }

  MemoryPool* pool_;
  std::shared_ptr<DataType> type_;
  int64_t length_;
  std::shared_ptr<ArrayData> out_;
  std::shared_ptr<Buffer> buffer_;
};

class RepeatedArrayFactory {
 public:
  RepeatedArrayFactory(MemoryPool* pool, const Scalar& scalar, int64_t length)
      : pool_(pool), scalar_(scalar), length_(length) {}

  Result<std::shared_ptr<Array>> Create() {
    RETURN_NOT_OK(VisitTypeInline(*scalar_.type, this));
    return out_;
  }

  Status Visit(const NullType&) { return Status::OK(); }

  Status Visit(const BooleanType&) {
    ARROW_ASSIGN_OR_RAISE(auto buffer, AllocateBitmap(length_, pool_));
    BitUtil::SetBitsTo(buffer->mutable_data(), 0, length_,
                       checked_cast<const BooleanScalar&>(scalar_).value);
    out_ = std::make_shared<BooleanArray>(length_, buffer);
    return Status::OK();
  }

  template <typename T>
  enable_if_number<T, Status> Visit(const T&) {
    auto value = checked_cast<const typename TypeTraits<T>::ScalarType&>(scalar_).value;
    return FinishFixedWidth(&value, sizeof(value));
  }

  template <typename T>
  enable_if_base_binary<T, Status> Visit(const T&) {
    std::shared_ptr<Buffer> value =
        checked_cast<const typename TypeTraits<T>::ScalarType&>(scalar_).value;
    std::shared_ptr<Buffer> values_buffer, offsets_buffer;
    RETURN_NOT_OK(CreateBufferOf(value->data(), value->size(), &values_buffer));
    auto size = static_cast<typename T::offset_type>(value->size());
    RETURN_NOT_OK(CreateOffsetsBuffer(size, &offsets_buffer));
    out_ = std::make_shared<typename TypeTraits<T>::ArrayType>(length_, offsets_buffer,
                                                               values_buffer);
    return Status::OK();
  }

  Status Visit(const FixedSizeBinaryType&) {
    std::shared_ptr<Buffer> value =
        checked_cast<const FixedSizeBinaryScalar&>(scalar_).value;
    return FinishFixedWidth(value->data(), value->size());
  }

  Status Visit(const Decimal128Type&) {
    auto value = checked_cast<const Decimal128Scalar&>(scalar_).value.ToBytes();
    return FinishFixedWidth(value.data(), value.size());
  }

  Status Visit(const DictionaryType& type) {
    const auto& value = checked_cast<const DictionaryScalar&>(scalar_).value;
    ARROW_ASSIGN_OR_RAISE(auto dictionary, MakeArrayFromScalar(*value, 1, pool_));

    ARROW_ASSIGN_OR_RAISE(auto zero, MakeScalar(type.index_type(), 0));
    ARROW_ASSIGN_OR_RAISE(auto indices, MakeArrayFromScalar(*zero, length_, pool_));

    out_ = std::make_shared<DictionaryArray>(scalar_.type, std::move(indices),
                                             std::move(dictionary));
    return Status::OK();
  }

  Status Visit(const DataType& type) {
    return Status::NotImplemented("construction from scalar of type ", *scalar_.type);
  }

  template <typename OffsetType>
  Status CreateOffsetsBuffer(OffsetType value_length, std::shared_ptr<Buffer>* out) {
    TypedBufferBuilder<OffsetType> builder(pool_);
    RETURN_NOT_OK(builder.Resize(length_ + 1));
    OffsetType offset = 0;
    for (int64_t i = 0; i < length_ + 1; ++i, offset += value_length) {
      builder.UnsafeAppend(offset);
    }
    return builder.Finish(out);
  }

  Status CreateBufferOf(const void* data, size_t data_length,
                        std::shared_ptr<Buffer>* out) {
    BufferBuilder builder(pool_);
    RETURN_NOT_OK(builder.Resize(length_ * data_length));
    for (int64_t i = 0; i < length_; ++i) {
      builder.UnsafeAppend(data, data_length);
    }
    return builder.Finish(out);
  }

  Status FinishFixedWidth(const void* data, size_t data_length) {
    std::shared_ptr<Buffer> buffer;
    RETURN_NOT_OK(CreateBufferOf(data, data_length, &buffer));
    out_ = MakeArray(
        ArrayData::Make(scalar_.type, length_, {nullptr, std::move(buffer)}, 0));
    return Status::OK();
  }

  MemoryPool* pool_;
  const Scalar& scalar_;
  int64_t length_;
  std::shared_ptr<Array> out_;
};

}  // namespace internal

Result<std::shared_ptr<Array>> MakeArrayOfNull(const std::shared_ptr<DataType>& type,
                                               int64_t length, MemoryPool* pool) {
  ARROW_ASSIGN_OR_RAISE(auto data,
                        internal::NullArrayFactory(pool, type, length).Create());
  return MakeArray(data);
}

Result<std::shared_ptr<Array>> MakeArrayFromScalar(const Scalar& scalar, int64_t length,
                                                   MemoryPool* pool) {
  if (!scalar.is_valid) {
    return MakeArrayOfNull(scalar.type, length, pool);
  }
  return internal::RepeatedArrayFactory(pool, scalar, length).Create();
}

Status MakeArrayOfNull(MemoryPool* pool, const std::shared_ptr<DataType>& type,
                       int64_t length, std::shared_ptr<Array>* out) {
  return MakeArrayOfNull(type, length, pool).Value(out);
}

Status MakeArrayOfNull(const std::shared_ptr<DataType>& type, int64_t length,
                       std::shared_ptr<Array>* out) {
  return MakeArrayOfNull(type, length).Value(out);
}

Status MakeArrayFromScalar(MemoryPool* pool, const Scalar& scalar, int64_t length,
                           std::shared_ptr<Array>* out) {
  return MakeArrayFromScalar(scalar, length, pool).Value(out);
}

Status MakeArrayFromScalar(const Scalar& scalar, int64_t length,
                           std::shared_ptr<Array>* out) {
  return MakeArrayFromScalar(scalar, length).Value(out);
}

namespace internal {

std::vector<ArrayVector> RechunkArraysConsistently(
    const std::vector<ArrayVector>& groups) {
  if (groups.size() <= 1) {
    return groups;
  }
  int64_t total_length = 0;
  for (const auto& array : groups.front()) {
    total_length += array->length();
  }
#ifndef NDEBUG
  for (const auto& group : groups) {
    int64_t group_length = 0;
    for (const auto& array : group) {
      group_length += array->length();
    }
    DCHECK_EQ(group_length, total_length)
        << "Array groups should have the same total number of elements";
  }
#endif
  if (total_length == 0) {
    return groups;
  }

  // Set up result vectors
  std::vector<ArrayVector> rechunked_groups(groups.size());

  // Set up progress counters
  std::vector<ArrayVector::const_iterator> current_arrays;
  std::vector<int64_t> array_offsets;
  for (const auto& group : groups) {
    current_arrays.emplace_back(group.cbegin());
    array_offsets.emplace_back(0);
  }

  // Scan all array vectors at once, rechunking along the way
  int64_t start = 0;
  while (start < total_length) {
    // First compute max possible length for next chunk
    int64_t chunk_length = std::numeric_limits<int64_t>::max();
    for (size_t i = 0; i < groups.size(); i++) {
      auto& arr_it = current_arrays[i];
      auto& offset = array_offsets[i];
      // Skip any done arrays (including 0-length arrays)
      while (offset == (*arr_it)->length()) {
        ++arr_it;
        offset = 0;
      }
      const auto& array = *arr_it;
      DCHECK_GT(array->length(), offset);
      chunk_length = std::min(chunk_length, array->length() - offset);
    }
    DCHECK_GT(chunk_length, 0);

    // Then slice all arrays along this chunk size
    for (size_t i = 0; i < groups.size(); i++) {
      const auto& array = *current_arrays[i];
      auto& offset = array_offsets[i];
      if (offset == 0 && array->length() == chunk_length) {
        // Slice spans entire array
        rechunked_groups[i].emplace_back(array);
      } else {
        DCHECK_LT(chunk_length - offset, array->length());
        rechunked_groups[i].emplace_back(array->Slice(offset, chunk_length));
      }
      offset += chunk_length;
    }
    start += chunk_length;
  }

  return rechunked_groups;
}

}  // namespace internal
}  // namespace arrow
