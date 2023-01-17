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

#pragma once

#include <atomic>  // IWYU pragma: export
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "arrow/compare.h"
#include "arrow/type.h"
#include "arrow/type_fwd.h"
#include "arrow/type_traits.h"
#include "arrow/util/bit_util.h"
#include "arrow/util/checked_cast.h"
#include "arrow/util/macros.h"
#include "arrow/util/string_view.h"  // IWYU pragma: export
#include "arrow/util/visibility.h"

namespace arrow {

class Array;
class ArrayVisitor;

// When slicing, we do not know the null count of the sliced range without
// doing some computation. To avoid doing this eagerly, we set the null count
// to -1 (any negative number will do). When Array::null_count is called the
// first time, the null count will be computed. See ARROW-33
constexpr int64_t kUnknownNullCount = -1;

class MemoryPool;
class Status;

// ----------------------------------------------------------------------
// Generic array data container

/// \class ArrayData
/// \brief Mutable container for generic Arrow array data
///
/// This data structure is a self-contained representation of the memory and
/// metadata inside an Arrow array data structure (called vectors in Java). The
/// classes arrow::Array and its subclasses provide strongly-typed accessors
/// with support for the visitor pattern and other affordances.
///
/// This class is designed for easy internal data manipulation, analytical data
/// processing, and data transport to and from IPC messages. For example, we
/// could cast from int64 to float64 like so:
///
/// Int64Array arr = GetMyData();
/// auto new_data = arr.data()->Copy();
/// new_data->type = arrow::float64();
/// DoubleArray double_arr(new_data);
///
/// This object is also useful in an analytics setting where memory may be
/// reused. For example, if we had a group of operations all returning doubles,
/// say:
///
/// Log(Sqrt(Expr(arr)))
///
/// Then the low-level implementations of each of these functions could have
/// the signatures
///
/// void Log(const ArrayData& values, ArrayData* out);
///
/// As another example a function may consume one or more memory buffers in an
/// input array and replace them with newly-allocated data, changing the output
/// data type as well.
struct ARROW_EXPORT ArrayData {
  ArrayData() : length(0), null_count(0), offset(0) {}

  ArrayData(const std::shared_ptr<DataType>& type, int64_t length,
            int64_t null_count = kUnknownNullCount, int64_t offset = 0)
      : type(type), length(length), null_count(null_count), offset(offset) {}

  ArrayData(const std::shared_ptr<DataType>& type, int64_t length,
            std::vector<std::shared_ptr<Buffer>> buffers,
            int64_t null_count = kUnknownNullCount, int64_t offset = 0)
      : ArrayData(type, length, null_count, offset) {
    this->buffers = std::move(buffers);
  }

  ArrayData(const std::shared_ptr<DataType>& type, int64_t length,
            std::vector<std::shared_ptr<Buffer>> buffers,
            std::vector<std::shared_ptr<ArrayData>> child_data,
            int64_t null_count = kUnknownNullCount, int64_t offset = 0)
      : ArrayData(type, length, null_count, offset) {
    this->buffers = std::move(buffers);
    this->child_data = std::move(child_data);
  }

  static std::shared_ptr<ArrayData> Make(const std::shared_ptr<DataType>& type,
                                         int64_t length,
                                         std::vector<std::shared_ptr<Buffer>> buffers,
                                         int64_t null_count = kUnknownNullCount,
                                         int64_t offset = 0);

  static std::shared_ptr<ArrayData> Make(
      const std::shared_ptr<DataType>& type, int64_t length,
      std::vector<std::shared_ptr<Buffer>> buffers,
      std::vector<std::shared_ptr<ArrayData>> child_data,
      int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  static std::shared_ptr<ArrayData> Make(
      const std::shared_ptr<DataType>& type, int64_t length,
      std::vector<std::shared_ptr<Buffer>> buffers,
      std::vector<std::shared_ptr<ArrayData>> child_data,
      std::shared_ptr<Array> dictionary, int64_t null_count = kUnknownNullCount,
      int64_t offset = 0);

  static std::shared_ptr<ArrayData> Make(const std::shared_ptr<DataType>& type,
                                         int64_t length,
                                         int64_t null_count = kUnknownNullCount,
                                         int64_t offset = 0);

  // Move constructor
  ArrayData(ArrayData&& other) noexcept
      : type(std::move(other.type)),
        length(other.length),
        offset(other.offset),
        buffers(std::move(other.buffers)),
        child_data(std::move(other.child_data)),
        dictionary(std::move(other.dictionary)) {
    SetNullCount(other.null_count);
  }

  // Copy constructor
  ArrayData(const ArrayData& other) noexcept
      : type(other.type),
        length(other.length),
        offset(other.offset),
        buffers(other.buffers),
        child_data(other.child_data),
        dictionary(other.dictionary) {
    SetNullCount(other.null_count);
  }

  // Move assignment
  ArrayData& operator=(ArrayData&& other) {
    type = std::move(other.type);
    length = other.length;
    SetNullCount(other.null_count);
    offset = other.offset;
    buffers = std::move(other.buffers);
    child_data = std::move(other.child_data);
    dictionary = std::move(other.dictionary);
    return *this;
  }

  // Copy assignment
  ArrayData& operator=(const ArrayData& other) {
    type = other.type;
    length = other.length;
    SetNullCount(other.null_count);
    offset = other.offset;
    buffers = other.buffers;
    child_data = other.child_data;
    dictionary = other.dictionary;
    return *this;
  }

  std::shared_ptr<ArrayData> Copy() const { return std::make_shared<ArrayData>(*this); }

  // Access a buffer's data as a typed C pointer
  template <typename T>
  inline const T* GetValues(int i, int64_t absolute_offset) const {
    if (buffers[i]) {
      return reinterpret_cast<const T*>(buffers[i]->data()) + absolute_offset;
    } else {
      return NULLPTR;
    }
  }

  template <typename T>
  inline const T* GetValues(int i) const {
    return GetValues<T>(i, offset);
  }

  // Access a buffer's data as a typed C pointer
  template <typename T>
  inline T* GetMutableValues(int i, int64_t absolute_offset) {
    if (buffers[i]) {
      return reinterpret_cast<T*>(buffers[i]->mutable_data()) + absolute_offset;
    } else {
      return NULLPTR;
    }
  }

  template <typename T>
  inline T* GetMutableValues(int i) {
    return GetMutableValues<T>(i, offset);
  }

  // Construct a zero-copy slice of the data with the indicated offset and length
  ArrayData Slice(int64_t offset, int64_t length) const;

  void SetNullCount(int64_t v) { null_count.store(v); }

  /// \brief Return null count, or compute and set it if it's not known
  int64_t GetNullCount() const;

  std::shared_ptr<DataType> type;
  int64_t length;
  mutable std::atomic<int64_t> null_count;
  // The logical start point into the physical buffers (in values, not bytes).
  // Note that, for child data, this must be *added* to the child data's own offset.
  int64_t offset;
  std::vector<std::shared_ptr<Buffer>> buffers;
  std::vector<std::shared_ptr<ArrayData>> child_data;

  // The dictionary for this Array, if any. Only used for dictionary
  // type
  std::shared_ptr<Array> dictionary;
};

/// \brief Create a strongly-typed Array instance from generic ArrayData
/// \param[in] data the array contents
/// \return the resulting Array instance
ARROW_EXPORT
std::shared_ptr<Array> MakeArray(const std::shared_ptr<ArrayData>& data);

/// \brief Create a strongly-typed Array instance with all elements null
/// \param[in] type the array type
/// \param[in] length the array length
/// \param[in] pool the memory pool to allocate memory from
ARROW_EXPORT
Result<std::shared_ptr<Array>> MakeArrayOfNull(const std::shared_ptr<DataType>& type,
                                               int64_t length,
                                               MemoryPool* pool = default_memory_pool());

/// \brief Create an Array instance whose slots are the given scalar
/// \param[in] scalar the value with which to fill the array
/// \param[in] length the array length
/// \param[in] pool the memory pool to allocate memory from
ARROW_EXPORT
Result<std::shared_ptr<Array>> MakeArrayFromScalar(
    const Scalar& scalar, int64_t length, MemoryPool* pool = default_memory_pool());

/// \brief Create a strongly-typed Array instance with all elements null
/// \param[in] type the array type
/// \param[in] length the array length
/// \param[out] out resulting Array instance
ARROW_DEPRECATED("Use Result-returning version")
ARROW_EXPORT
Status MakeArrayOfNull(const std::shared_ptr<DataType>& type, int64_t length,
                       std::shared_ptr<Array>* out);

/// \brief Create a strongly-typed Array instance with all elements null
/// \param[in] pool the pool from which memory for this array will be allocated
/// \param[in] type the array type
/// \param[in] length the array length
/// \param[out] out resulting Array instance
ARROW_DEPRECATED("Use Result-returning version")
ARROW_EXPORT
Status MakeArrayOfNull(MemoryPool* pool, const std::shared_ptr<DataType>& type,
                       int64_t length, std::shared_ptr<Array>* out);

/// \brief Create an Array instance whose slots are the given scalar
/// \param[in] scalar the value with which to fill the array
/// \param[in] length the array length
/// \param[out] out resulting Array instance
ARROW_DEPRECATED("Use Result-returning version")
ARROW_EXPORT
Status MakeArrayFromScalar(const Scalar& scalar, int64_t length,
                           std::shared_ptr<Array>* out);

/// \brief Create a strongly-typed Array instance with all elements null
/// \param[in] pool the pool from which memory for this array will be allocated
/// \param[in] scalar the value with which to fill the array
/// \param[in] length the array length
/// \param[out] out resulting Array instance
ARROW_DEPRECATED("Use Result-returning version")
ARROW_EXPORT
Status MakeArrayFromScalar(MemoryPool* pool, const Scalar& scalar, int64_t length,
                           std::shared_ptr<Array>* out);

// ----------------------------------------------------------------------
// User array accessor types

/// \brief Array base type
/// Immutable data array with some logical type and some length.
///
/// Any memory is owned by the respective Buffer instance (or its parents).
///
/// The base class is only required to have a null bitmap buffer if the null
/// count is greater than 0
///
/// If known, the null count can be provided in the base Array constructor. If
/// the null count is not known, pass -1 to indicate that the null count is to
/// be computed on the first call to null_count()
class ARROW_EXPORT Array {
 public:
  virtual ~Array() = default;

  /// \brief Return true if value at index is null. Does not boundscheck
  bool IsNull(int64_t i) const {
    return null_bitmap_data_ != NULLPTR &&
           !BitUtil::GetBit(null_bitmap_data_, i + data_->offset);
  }

  /// \brief Return true if value at index is valid (not null). Does not
  /// boundscheck
  bool IsValid(int64_t i) const {
    return null_bitmap_data_ == NULLPTR ||
           BitUtil::GetBit(null_bitmap_data_, i + data_->offset);
  }

  /// Size in the number of elements this array contains.
  int64_t length() const { return data_->length; }

  /// A relative position into another array's data, to enable zero-copy
  /// slicing. This value defaults to zero
  int64_t offset() const { return data_->offset; }

  /// The number of null entries in the array. If the null count was not known
  /// at time of construction (and set to a negative value), then the null
  /// count will be computed and cached on the first invocation of this
  /// function
  int64_t null_count() const;

  std::shared_ptr<DataType> type() const { return data_->type; }
  Type::type type_id() const { return data_->type->id(); }

  /// Buffer for the null bitmap.
  ///
  /// Note that for `null_count == 0`, this can be null.
  /// This buffer does not account for any slice offset
  std::shared_ptr<Buffer> null_bitmap() const { return data_->buffers[0]; }

  /// Raw pointer to the null bitmap.
  ///
  /// Note that for `null_count == 0`, this can be null.
  /// This buffer does not account for any slice offset
  const uint8_t* null_bitmap_data() const { return null_bitmap_data_; }

  /// Equality comparison with another array
  bool Equals(const Array& arr, const EqualOptions& = EqualOptions::Defaults()) const;
  bool Equals(const std::shared_ptr<Array>& arr,
              const EqualOptions& = EqualOptions::Defaults()) const;

  /// \brief Return the formatted unified diff of arrow::Diff between this
  /// Array and another Array
  std::string Diff(const Array& other) const;

  /// Approximate equality comparison with another array
  ///
  /// epsilon is only used if this is FloatArray or DoubleArray
  bool ApproxEquals(const std::shared_ptr<Array>& arr,
                    const EqualOptions& = EqualOptions::Defaults()) const;
  bool ApproxEquals(const Array& arr,
                    const EqualOptions& = EqualOptions::Defaults()) const;

  /// Compare if the range of slots specified are equal for the given array and
  /// this array.  end_idx exclusive.  This methods does not bounds check.
  bool RangeEquals(int64_t start_idx, int64_t end_idx, int64_t other_start_idx,
                   const Array& other) const;
  bool RangeEquals(int64_t start_idx, int64_t end_idx, int64_t other_start_idx,
                   const std::shared_ptr<Array>& other) const;
  bool RangeEquals(const Array& other, int64_t start_idx, int64_t end_idx,
                   int64_t other_start_idx) const;
  bool RangeEquals(const std::shared_ptr<Array>& other, int64_t start_idx,
                   int64_t end_idx, int64_t other_start_idx) const;

  Status Accept(ArrayVisitor* visitor) const;

  /// Construct a zero-copy view of this array with the given type.
  ///
  /// This method checks if the types are layout-compatible.
  /// Nested types are traversed in depth-first order. Data buffers must have
  /// the same item sizes, even though the logical types may be different.
  /// An error is returned if the types are not layout-compatible.
  Result<std::shared_ptr<Array>> View(const std::shared_ptr<DataType>& type) const;

  ARROW_DEPRECATED("Use Result-returning version")
  Status View(const std::shared_ptr<DataType>& type, std::shared_ptr<Array>* out) const;

  /// Construct a zero-copy slice of the array with the indicated offset and
  /// length
  ///
  /// \param[in] offset the position of the first element in the constructed
  /// slice
  /// \param[in] length the length of the slice. If there are not enough
  /// elements in the array, the length will be adjusted accordingly
  ///
  /// \return a new object wrapped in std::shared_ptr<Array>
  std::shared_ptr<Array> Slice(int64_t offset, int64_t length) const;

  /// Slice from offset until end of the array
  std::shared_ptr<Array> Slice(int64_t offset) const;

  std::shared_ptr<ArrayData> data() const { return data_; }

  int num_fields() const { return static_cast<int>(data_->child_data.size()); }

  /// \return PrettyPrint representation of array suitable for debugging
  std::string ToString() const;

  /// \brief Perform cheap validation checks to determine obvious inconsistencies
  /// within the array's internal data.
  ///
  /// This is O(k) where k is the number of descendents.
  ///
  /// \return Status
  Status Validate() const;

  /// \brief Perform extensive validation checks to determine inconsistencies
  /// within the array's internal data.
  ///
  /// This is potentially O(k*n) where k is the number of descendents and n
  /// is the array length.
  ///
  /// \return Status
  Status ValidateFull() const;

 protected:
  Array() : null_bitmap_data_(NULLPTR) {}

  std::shared_ptr<ArrayData> data_;
  const uint8_t* null_bitmap_data_;

  /// Protected method for constructors
  inline void SetData(const std::shared_ptr<ArrayData>& data) {
    if (data->buffers.size() > 0 && data->buffers[0]) {
      null_bitmap_data_ = data->buffers[0]->data();
    } else {
      null_bitmap_data_ = NULLPTR;
    }
    data_ = data;
  }

 private:
  ARROW_DISALLOW_COPY_AND_ASSIGN(Array);
};

namespace internal {

/// Given a number of ArrayVectors, treat each ArrayVector as the
/// chunks of a chunked array.  Then rechunk each ArrayVector such that
/// all ArrayVectors are chunked identically.  It is mandatory that
/// all ArrayVectors contain the same total number of elements.
ARROW_EXPORT
std::vector<ArrayVector> RechunkArraysConsistently(const std::vector<ArrayVector>&);

}  // namespace internal

static inline std::ostream& operator<<(std::ostream& os, const Array& x) {
  os << x.ToString();
  return os;
}

/// Base class for non-nested arrays
class ARROW_EXPORT FlatArray : public Array {
 protected:
  using Array::Array;
};

/// Degenerate null type Array
class ARROW_EXPORT NullArray : public FlatArray {
 public:
  using TypeClass = NullType;

  explicit NullArray(const std::shared_ptr<ArrayData>& data) { SetData(data); }
  explicit NullArray(int64_t length);

 private:
  inline void SetData(const std::shared_ptr<ArrayData>& data) {
    null_bitmap_data_ = NULLPTR;
    data->null_count = data->length;
    data_ = data;
  }
};

/// Base class for arrays of fixed-size logical types
class ARROW_EXPORT PrimitiveArray : public FlatArray {
 public:
  PrimitiveArray(const std::shared_ptr<DataType>& type, int64_t length,
                 const std::shared_ptr<Buffer>& data,
                 const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
                 int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  /// Does not account for any slice offset
  std::shared_ptr<Buffer> values() const { return data_->buffers[1]; }

 protected:
  PrimitiveArray() : raw_values_(NULLPTR) {}

  inline void SetData(const std::shared_ptr<ArrayData>& data) {
    auto values = data->buffers[1];
    this->Array::SetData(data);
    raw_values_ = values == NULLPTR ? NULLPTR : values->data();
  }

  explicit inline PrimitiveArray(const std::shared_ptr<ArrayData>& data) {
    SetData(data);
  }

  const uint8_t* raw_values_;
};

/// Concrete Array class for numeric data.
template <typename TYPE>
class NumericArray : public PrimitiveArray {
 public:
  using TypeClass = TYPE;
  using value_type = typename TypeClass::c_type;

  explicit NumericArray(const std::shared_ptr<ArrayData>& data) : PrimitiveArray(data) {}

  // Only enable this constructor without a type argument for types without additional
  // metadata
  template <typename T1 = TYPE>
  NumericArray(enable_if_parameter_free<T1, int64_t> length,
               const std::shared_ptr<Buffer>& data,
               const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
               int64_t null_count = kUnknownNullCount, int64_t offset = 0)
      : PrimitiveArray(TypeTraits<T1>::type_singleton(), length, data, null_bitmap,
                       null_count, offset) {}

  const value_type* raw_values() const {
    return reinterpret_cast<const value_type*>(raw_values_) + data_->offset;
  }

  value_type Value(int64_t i) const { return raw_values()[i]; }

  // For API compatibility with BinaryArray etc.
  value_type GetView(int64_t i) const { return Value(i); }

 protected:
  using PrimitiveArray::PrimitiveArray;
};

/// Concrete Array class for boolean data
class ARROW_EXPORT BooleanArray : public PrimitiveArray {
 public:
  using TypeClass = BooleanType;

  explicit BooleanArray(const std::shared_ptr<ArrayData>& data);

  BooleanArray(int64_t length, const std::shared_ptr<Buffer>& data,
               const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
               int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  bool Value(int64_t i) const {
    return BitUtil::GetBit(reinterpret_cast<const uint8_t*>(raw_values_),
                           i + data_->offset);
  }

  bool GetView(int64_t i) const { return Value(i); }

 protected:
  using PrimitiveArray::PrimitiveArray;
};

// ----------------------------------------------------------------------
// ListArray

/// Base class for variable-sized list arrays, regardless of offset size.
template <typename TYPE>
class BaseListArray : public Array {
 public:
  using TypeClass = TYPE;
  using offset_type = typename TypeClass::offset_type;

  const TypeClass* list_type() const { return list_type_; }

  /// \brief Return array object containing the list's values
  std::shared_ptr<Array> values() const { return values_; }

  /// Note that this buffer does not account for any slice offset
  std::shared_ptr<Buffer> value_offsets() const { return data_->buffers[1]; }

  std::shared_ptr<DataType> value_type() const { return list_type_->value_type(); }

  /// Return pointer to raw value offsets accounting for any slice offset
  const offset_type* raw_value_offsets() const {
    return raw_value_offsets_ + data_->offset;
  }

  // The following functions will not perform boundschecking
  offset_type value_offset(int64_t i) const {
    return raw_value_offsets_[i + data_->offset];
  }
  offset_type value_length(int64_t i) const {
    i += data_->offset;
    return raw_value_offsets_[i + 1] - raw_value_offsets_[i];
  }
  std::shared_ptr<Array> value_slice(int64_t i) const {
    return values_->Slice(value_offset(i), value_length(i));
  }

 protected:
  const TypeClass* list_type_ = NULLPTR;
  std::shared_ptr<Array> values_;
  const offset_type* raw_value_offsets_ = NULLPTR;
};

/// Concrete Array class for list data
class ARROW_EXPORT ListArray : public BaseListArray<ListType> {
 public:
  explicit ListArray(const std::shared_ptr<ArrayData>& data);

  ListArray(const std::shared_ptr<DataType>& type, int64_t length,
            const std::shared_ptr<Buffer>& value_offsets,
            const std::shared_ptr<Array>& values,
            const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
            int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  /// \brief Construct ListArray from array of offsets and child value array
  ///
  /// This function does the bare minimum of validation of the offsets and
  /// input types, and will allocate a new offsets array if necessary (i.e. if
  /// the offsets contain any nulls). If the offsets do not have nulls, they
  /// are assumed to be well-formed
  ///
  /// \param[in] offsets Array containing n + 1 offsets encoding length and
  /// size. Must be of int32 type
  /// \param[in] values Array containing list values
  /// \param[in] pool MemoryPool in case new offsets array needs to be
  /// allocated because of null values
  static Result<std::shared_ptr<Array>> FromArrays(
      const Array& offsets, const Array& values,
      MemoryPool* pool = default_memory_pool());

  ARROW_DEPRECATED("Use Result-returning version")
  static Status FromArrays(const Array& offsets, const Array& values, MemoryPool* pool,
                           std::shared_ptr<Array>* out);

  /// \brief Return an Array that is a concatenation of the lists in this array.
  ///
  /// Note that it's different from `values()` in that it takes into
  /// consideration of this array's offsets as well as null elements backed
  /// by non-empty lists (they are skipped, thus copying may be needed).
  Result<std::shared_ptr<Array>> Flatten(
      MemoryPool* memory_pool = default_memory_pool()) const;

 protected:
  // This constructor defers SetData to a derived array class
  ListArray() = default;
  void SetData(const std::shared_ptr<ArrayData>& data,
               Type::type expected_type_id = Type::LIST);
};

/// Concrete Array class for large list data (with 64-bit offsets)
class ARROW_EXPORT LargeListArray : public BaseListArray<LargeListType> {
 public:
  explicit LargeListArray(const std::shared_ptr<ArrayData>& data);

  LargeListArray(const std::shared_ptr<DataType>& type, int64_t length,
                 const std::shared_ptr<Buffer>& value_offsets,
                 const std::shared_ptr<Array>& values,
                 const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
                 int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  /// \brief Construct LargeListArray from array of offsets and child value array
  ///
  /// This function does the bare minimum of validation of the offsets and
  /// input types, and will allocate a new offsets array if necessary (i.e. if
  /// the offsets contain any nulls). If the offsets do not have nulls, they
  /// are assumed to be well-formed
  ///
  /// \param[in] offsets Array containing n + 1 offsets encoding length and
  /// size. Must be of int64 type
  /// \param[in] values Array containing list values
  /// \param[in] pool MemoryPool in case new offsets array needs to be
  /// allocated because of null values
  static Result<std::shared_ptr<Array>> FromArrays(
      const Array& offsets, const Array& values,
      MemoryPool* pool = default_memory_pool());

  ARROW_DEPRECATED("Use Result-returning version")
  static Status FromArrays(const Array& offsets, const Array& values, MemoryPool* pool,
                           std::shared_ptr<Array>* out);

  /// \brief Return an Array that is a concatenation of the lists in this array.
  ///
  /// Note that it's different from `values()` in that it takes into
  /// consideration of this array's offsets as well as null elements backed
  /// by non-empty lists (they are skipped, thus copying may be needed).
  Result<std::shared_ptr<Array>> Flatten(
      MemoryPool* memory_pool = default_memory_pool()) const;

 protected:
  void SetData(const std::shared_ptr<ArrayData>& data);
};

// ----------------------------------------------------------------------
// MapArray

/// Concrete Array class for map data
///
/// NB: "value" in this context refers to a pair of a key and the corresponding item
class ARROW_EXPORT MapArray : public ListArray {
 public:
  using TypeClass = MapType;

  explicit MapArray(const std::shared_ptr<ArrayData>& data);

  MapArray(const std::shared_ptr<DataType>& type, int64_t length,
           const std::shared_ptr<Buffer>& value_offsets,
           const std::shared_ptr<Array>& keys, const std::shared_ptr<Array>& items,
           const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
           int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  MapArray(const std::shared_ptr<DataType>& type, int64_t length,
           const std::shared_ptr<Buffer>& value_offsets,
           const std::shared_ptr<Array>& values,
           const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
           int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  /// \brief Construct MapArray from array of offsets and child key, item arrays
  ///
  /// This function does the bare minimum of validation of the offsets and
  /// input types, and will allocate a new offsets array if necessary (i.e. if
  /// the offsets contain any nulls). If the offsets do not have nulls, they
  /// are assumed to be well-formed
  ///
  /// \param[in] offsets Array containing n + 1 offsets encoding length and
  /// size. Must be of int32 type
  /// \param[in] keys Array containing key values
  /// \param[in] items Array containing item values
  /// \param[in] pool MemoryPool in case new offsets array needs to be
  /// allocated because of null values
  static Result<std::shared_ptr<Array>> FromArrays(
      const std::shared_ptr<Array>& offsets, const std::shared_ptr<Array>& keys,
      const std::shared_ptr<Array>& items, MemoryPool* pool = default_memory_pool());

  ARROW_DEPRECATED("Use Result-returning version")
  static Status FromArrays(const std::shared_ptr<Array>& offsets,
                           const std::shared_ptr<Array>& keys,
                           const std::shared_ptr<Array>& items, MemoryPool* pool,
                           std::shared_ptr<Array>* out);

  const MapType* map_type() const { return map_type_; }

  /// \brief Return array object containing all map keys
  std::shared_ptr<Array> keys() const { return keys_; }

  /// \brief Return array object containing all mapped items
  std::shared_ptr<Array> items() const { return items_; }

  /// Validate child data before constructing the actual MapArray.
  static Status ValidateChildData(
      const std::vector<std::shared_ptr<ArrayData>>& child_data);

 protected:
  void SetData(const std::shared_ptr<ArrayData>& data);

 private:
  const MapType* map_type_;
  std::shared_ptr<Array> keys_, items_;
};

// ----------------------------------------------------------------------
// FixedSizeListArray

/// Concrete Array class for fixed size list data
class ARROW_EXPORT FixedSizeListArray : public Array {
 public:
  using TypeClass = FixedSizeListType;
  using offset_type = TypeClass::offset_type;

  explicit FixedSizeListArray(const std::shared_ptr<ArrayData>& data);

  FixedSizeListArray(const std::shared_ptr<DataType>& type, int64_t length,
                     const std::shared_ptr<Array>& values,
                     const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
                     int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  const FixedSizeListType* list_type() const;

  /// \brief Return array object containing the list's values
  std::shared_ptr<Array> values() const;

  std::shared_ptr<DataType> value_type() const;

  // The following functions will not perform boundschecking
  int32_t value_offset(int64_t i) const {
    i += data_->offset;
    return static_cast<int32_t>(list_size_ * i);
  }
  int32_t value_length(int64_t i = 0) const { return list_size_; }
  std::shared_ptr<Array> value_slice(int64_t i) const {
    return values_->Slice(value_offset(i), value_length(i));
  }

  /// \brief Construct FixedSizeListArray from child value array and value_length
  ///
  /// \param[in] values Array containing list values
  /// \param[in] list_size The fixed length of each list
  /// \return Will have length equal to values.length() / list_size
  static Result<std::shared_ptr<Array>> FromArrays(const std::shared_ptr<Array>& values,
                                                   int32_t list_size);

 protected:
  void SetData(const std::shared_ptr<ArrayData>& data);
  int32_t list_size_;

 private:
  std::shared_ptr<Array> values_;
};

// ----------------------------------------------------------------------
// Binary and String

/// Base class for variable-sized binary arrays, regardless of offset size
/// and logical interpretation.
template <typename TYPE>
class BaseBinaryArray : public FlatArray {
 public:
  using TypeClass = TYPE;
  using offset_type = typename TypeClass::offset_type;

  /// Return the pointer to the given elements bytes
  // XXX should GetValue(int64_t i) return a string_view?
  const uint8_t* GetValue(int64_t i, offset_type* out_length) const {
    // Account for base offset
    i += data_->offset;
    const offset_type pos = raw_value_offsets_[i];
    *out_length = raw_value_offsets_[i + 1] - pos;
    return raw_data_ + pos;
  }

  /// \brief Get binary value as a string_view
  ///
  /// \param i the value index
  /// \return the view over the selected value
  util::string_view GetView(int64_t i) const {
    // Account for base offset
    i += data_->offset;
    const offset_type pos = raw_value_offsets_[i];
    return util::string_view(reinterpret_cast<const char*>(raw_data_ + pos),
                             raw_value_offsets_[i + 1] - pos);
  }

  /// \brief Get binary value as a std::string
  ///
  /// \param i the value index
  /// \return the value copied into a std::string
  std::string GetString(int64_t i) const { return std::string(GetView(i)); }

  /// Note that this buffer does not account for any slice offset
  std::shared_ptr<Buffer> value_offsets() const { return data_->buffers[1]; }

  /// Note that this buffer does not account for any slice offset
  std::shared_ptr<Buffer> value_data() const { return data_->buffers[2]; }

  const offset_type* raw_value_offsets() const {
    return raw_value_offsets_ + data_->offset;
  }

  // Neither of these functions will perform boundschecking
  offset_type value_offset(int64_t i) const {
    return raw_value_offsets_[i + data_->offset];
  }
  offset_type value_length(int64_t i) const {
    i += data_->offset;
    return raw_value_offsets_[i + 1] - raw_value_offsets_[i];
  }

 protected:
  // For subclasses
  BaseBinaryArray() : raw_value_offsets_(NULLPTR), raw_data_(NULLPTR) {}

  // Protected method for constructors
  void SetData(const std::shared_ptr<ArrayData>& data) {
    auto value_offsets = data->buffers[1];
    auto value_data = data->buffers[2];
    this->Array::SetData(data);
    raw_data_ = value_data == NULLPTR ? NULLPTR : value_data->data();
    raw_value_offsets_ =
        value_offsets == NULLPTR
            ? NULLPTR
            : reinterpret_cast<const offset_type*>(value_offsets->data());
  }

  const offset_type* raw_value_offsets_;
  const uint8_t* raw_data_;
};

/// Concrete Array class for variable-size binary data
class ARROW_EXPORT BinaryArray : public BaseBinaryArray<BinaryType> {
 public:
  explicit BinaryArray(const std::shared_ptr<ArrayData>& data);

  BinaryArray(int64_t length, const std::shared_ptr<Buffer>& value_offsets,
              const std::shared_ptr<Buffer>& data,
              const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
              int64_t null_count = kUnknownNullCount, int64_t offset = 0);

 protected:
  // For subclasses such as StringArray
  BinaryArray() : BaseBinaryArray() {}
};

/// Concrete Array class for variable-size string (utf-8) data
class ARROW_EXPORT StringArray : public BinaryArray {
 public:
  using TypeClass = StringType;

  explicit StringArray(const std::shared_ptr<ArrayData>& data);

  StringArray(int64_t length, const std::shared_ptr<Buffer>& value_offsets,
              const std::shared_ptr<Buffer>& data,
              const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
              int64_t null_count = kUnknownNullCount, int64_t offset = 0);
};

/// Concrete Array class for large variable-size binary data
class ARROW_EXPORT LargeBinaryArray : public BaseBinaryArray<LargeBinaryType> {
 public:
  explicit LargeBinaryArray(const std::shared_ptr<ArrayData>& data);

  LargeBinaryArray(int64_t length, const std::shared_ptr<Buffer>& value_offsets,
                   const std::shared_ptr<Buffer>& data,
                   const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
                   int64_t null_count = kUnknownNullCount, int64_t offset = 0);

 protected:
  // For subclasses such as LargeStringArray
  LargeBinaryArray() : BaseBinaryArray() {}
};

/// Concrete Array class for large variable-size string (utf-8) data
class ARROW_EXPORT LargeStringArray : public LargeBinaryArray {
 public:
  using TypeClass = LargeStringType;

  explicit LargeStringArray(const std::shared_ptr<ArrayData>& data);

  LargeStringArray(int64_t length, const std::shared_ptr<Buffer>& value_offsets,
                   const std::shared_ptr<Buffer>& data,
                   const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
                   int64_t null_count = kUnknownNullCount, int64_t offset = 0);
};

// ----------------------------------------------------------------------
// Fixed width binary

/// Concrete Array class for fixed-size binary data
class ARROW_EXPORT FixedSizeBinaryArray : public PrimitiveArray {
 public:
  using TypeClass = FixedSizeBinaryType;

  explicit FixedSizeBinaryArray(const std::shared_ptr<ArrayData>& data);

  FixedSizeBinaryArray(const std::shared_ptr<DataType>& type, int64_t length,
                       const std::shared_ptr<Buffer>& data,
                       const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
                       int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  const uint8_t* GetValue(int64_t i) const;
  const uint8_t* Value(int64_t i) const { return GetValue(i); }

  util::string_view GetView(int64_t i) const {
    return util::string_view(reinterpret_cast<const char*>(GetValue(i)), byte_width());
  }

  std::string GetString(int64_t i) const { return std::string(GetView(i)); }

  int32_t byte_width() const { return byte_width_; }

  const uint8_t* raw_values() const { return raw_values_ + data_->offset * byte_width_; }

 protected:
  inline void SetData(const std::shared_ptr<ArrayData>& data) {
    this->PrimitiveArray::SetData(data);
    byte_width_ =
        internal::checked_cast<const FixedSizeBinaryType&>(*type()).byte_width();
  }

  int32_t byte_width_;
};

/// DayTimeArray
/// ---------------------
/// \brief Array of Day and Millisecond values.
class ARROW_EXPORT DayTimeIntervalArray : public PrimitiveArray {
 public:
  using TypeClass = DayTimeIntervalType;

  explicit DayTimeIntervalArray(const std::shared_ptr<ArrayData>& data);

  DayTimeIntervalArray(const std::shared_ptr<DataType>& type, int64_t length,
                       const std::shared_ptr<Buffer>& data,
                       const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
                       int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  TypeClass::DayMilliseconds GetValue(int64_t i) const;
  TypeClass::DayMilliseconds Value(int64_t i) const { return GetValue(i); }

  // For compatibility with Take kernel.
  TypeClass::DayMilliseconds GetView(int64_t i) const { return GetValue(i); }

  int32_t byte_width() const { return sizeof(TypeClass::DayMilliseconds); }

  const uint8_t* raw_values() const { return raw_values_ + data_->offset * byte_width(); }

 protected:
  inline void SetData(const std::shared_ptr<ArrayData>& data) {
    this->PrimitiveArray::SetData(data);
  }
};

// ----------------------------------------------------------------------
// Decimal128Array

/// Concrete Array class for 128-bit decimal data
class ARROW_EXPORT Decimal128Array : public FixedSizeBinaryArray {
 public:
  using TypeClass = Decimal128Type;

  using FixedSizeBinaryArray::FixedSizeBinaryArray;

  /// \brief Construct Decimal128Array from ArrayData instance
  explicit Decimal128Array(const std::shared_ptr<ArrayData>& data);

  std::string FormatValue(int64_t i) const;
};

// Backward compatibility
using DecimalArray = Decimal128Array;

// ----------------------------------------------------------------------
// Struct

/// Concrete Array class for struct data
class ARROW_EXPORT StructArray : public Array {
 public:
  using TypeClass = StructType;

  explicit StructArray(const std::shared_ptr<ArrayData>& data);

  StructArray(const std::shared_ptr<DataType>& type, int64_t length,
              const std::vector<std::shared_ptr<Array>>& children,
              std::shared_ptr<Buffer> null_bitmap = NULLPTR,
              int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  /// \brief Return a StructArray from child arrays and field names.
  ///
  /// The length and data type are automatically inferred from the arguments.
  /// There should be at least one child array.
  static Result<std::shared_ptr<StructArray>> Make(
      const std::vector<std::shared_ptr<Array>>& children,
      const std::vector<std::string>& field_names,
      std::shared_ptr<Buffer> null_bitmap = NULLPTR,
      int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  /// \brief Return a StructArray from child arrays and fields.
  ///
  /// The length is automatically inferred from the arguments.
  /// There should be at least one child array.  This method does not
  /// check that field types and child array types are consistent.
  static Result<std::shared_ptr<StructArray>> Make(
      const std::vector<std::shared_ptr<Array>>& children,
      const std::vector<std::shared_ptr<Field>>& fields,
      std::shared_ptr<Buffer> null_bitmap = NULLPTR,
      int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  const StructType* struct_type() const;

  // Return a shared pointer in case the requestor desires to share ownership
  // with this array.  The returned array has its offset, length and null
  // count adjusted.
  std::shared_ptr<Array> field(int pos) const;

  /// Returns null if name not found
  std::shared_ptr<Array> GetFieldByName(const std::string& name) const;

  /// \brief Flatten this array as a vector of arrays, one for each field
  ///
  /// \param[in] pool The pool to allocate null bitmaps from, if necessary
  Result<ArrayVector> Flatten(MemoryPool* pool = default_memory_pool()) const;

  ARROW_DEPRECATED("Use Result-returning version")
  Status Flatten(MemoryPool* pool, ArrayVector* out) const;

 private:
  // For caching boxed child data
  // XXX This is not handled in a thread-safe manner.
  mutable std::vector<std::shared_ptr<Array>> boxed_fields_;
};

// ----------------------------------------------------------------------
// Union

/// Concrete Array class for union data
class ARROW_EXPORT UnionArray : public Array {
 public:
  using TypeClass = UnionType;

  using type_code_t = int8_t;

  explicit UnionArray(const std::shared_ptr<ArrayData>& data);

  UnionArray(const std::shared_ptr<DataType>& type, int64_t length,
             const std::vector<std::shared_ptr<Array>>& children,
             const std::shared_ptr<Buffer>& type_ids,
             const std::shared_ptr<Buffer>& value_offsets = NULLPTR,
             const std::shared_ptr<Buffer>& null_bitmap = NULLPTR,
             int64_t null_count = kUnknownNullCount, int64_t offset = 0);

  /// \brief Construct Dense UnionArray from types_ids, value_offsets and children
  ///
  /// This function does the bare minimum of validation of the offsets and
  /// input types. The value_offsets are assumed to be well-formed.
  ///
  /// \param[in] type_ids An array of logical type ids for the union type
  /// \param[in] value_offsets An array of signed int32 values indicating the
  /// relative offset into the respective child array for the type in a given slot.
  /// The respective offsets for each child value array must be in order / increasing.
  /// \param[in] children Vector of children Arrays containing the data for each type.
  /// \param[in] field_names Vector of strings containing the name of each field.
  /// \param[in] type_codes Vector of type codes.
  static Result<std::shared_ptr<Array>> MakeDense(
      const Array& type_ids, const Array& value_offsets,
      const std::vector<std::shared_ptr<Array>>& children,
      const std::vector<std::string>& field_names = {},
      const std::vector<type_code_t>& type_codes = {});

  /// \brief Construct Dense UnionArray from types_ids, value_offsets and children
  ///
  /// This function does the bare minimum of validation of the offsets and
  /// input types. The value_offsets are assumed to be well-formed.
  ///
  /// \param[in] type_ids An array of logical type ids for the union type
  /// \param[in] value_offsets An array of signed int32 values indicating the
  /// relative offset into the respective child array for the type in a given slot.
  /// The respective offsets for each child value array must be in order / increasing.
  /// \param[in] children Vector of children Arrays containing the data for each type.
  /// \param[in] type_codes Vector of type codes.
  static Result<std::shared_ptr<Array>> MakeDense(
      const Array& type_ids, const Array& value_offsets,
      const std::vector<std::shared_ptr<Array>>& children,
      const std::vector<type_code_t>& type_codes) {
    return MakeDense(type_ids, value_offsets, children, std::vector<std::string>{},
                     type_codes);
  }

  ARROW_DEPRECATED("Use Result-returning version")
  static Status MakeDense(const Array& type_ids, const Array& value_offsets,
                          const std::vector<std::shared_ptr<Array>>& children,
                          const std::vector<std::string>& field_names,
                          const std::vector<type_code_t>& type_codes,
                          std::shared_ptr<Array>* out) {
    return MakeDense(type_ids, value_offsets, children, field_names, type_codes)
        .Value(out);
  }

  ARROW_DEPRECATED("Use Result-returning version")
  static Status MakeDense(const Array& type_ids, const Array& value_offsets,
                          const std::vector<std::shared_ptr<Array>>& children,
                          const std::vector<std::string>& field_names,
                          std::shared_ptr<Array>* out) {
    return MakeDense(type_ids, value_offsets, children, field_names).Value(out);
  }

  ARROW_DEPRECATED("Use Result-returning version")
  static Status MakeDense(const Array& type_ids, const Array& value_offsets,
                          const std::vector<std::shared_ptr<Array>>& children,
                          const std::vector<type_code_t>& type_codes,
                          std::shared_ptr<Array>* out) {
    return MakeDense(type_ids, value_offsets, children, type_codes).Value(out);
  }

  ARROW_DEPRECATED("Use Result-returning version")
  static Status MakeDense(const Array& type_ids, const Array& value_offsets,
                          const std::vector<std::shared_ptr<Array>>& children,
                          std::shared_ptr<Array>* out) {
    return MakeDense(type_ids, value_offsets, children).Value(out);
  }

  /// \brief Construct Sparse UnionArray from type_ids and children
  ///
  /// This function does the bare minimum of validation of the offsets and
  /// input types.
  ///
  /// \param[in] type_ids An array of logical type ids for the union type
  /// \param[in] children Vector of children Arrays containing the data for each type.
  /// \param[in] field_names Vector of strings containing the name of each field.
  /// \param[in] type_codes Vector of type codes.
  static Result<std::shared_ptr<Array>> MakeSparse(
      const Array& type_ids, const std::vector<std::shared_ptr<Array>>& children,
      const std::vector<std::string>& field_names = {},
      const std::vector<type_code_t>& type_codes = {});

  /// \brief Construct Sparse UnionArray from type_ids and children
  ///
  /// This function does the bare minimum of validation of the offsets and
  /// input types.
  ///
  /// \param[in] type_ids An array of logical type ids for the union type
  /// \param[in] children Vector of children Arrays containing the data for each type.
  /// \param[in] type_codes Vector of type codes.
  static Result<std::shared_ptr<Array>> MakeSparse(
      const Array& type_ids, const std::vector<std::shared_ptr<Array>>& children,
      const std::vector<type_code_t>& type_codes) {
    return MakeSparse(type_ids, children, std::vector<std::string>{}, type_codes);
  }

  ARROW_DEPRECATED("Use Result-returning version")
  static Status MakeSparse(const Array& type_ids,
                           const std::vector<std::shared_ptr<Array>>& children,
                           const std::vector<std::string>& field_names,
                           const std::vector<type_code_t>& type_codes,
                           std::shared_ptr<Array>* out) {
    return MakeSparse(type_ids, children, field_names, type_codes).Value(out);
  }

  ARROW_DEPRECATED("Use Result-returning version")
  static Status MakeSparse(const Array& type_ids,
                           const std::vector<std::shared_ptr<Array>>& children,
                           const std::vector<std::string>& field_names,
                           std::shared_ptr<Array>* out) {
    return MakeSparse(type_ids, children, field_names).Value(out);
  }

  ARROW_DEPRECATED("Use Result-returning version")
  static Status MakeSparse(const Array& type_ids,
                           const std::vector<std::shared_ptr<Array>>& children,
                           const std::vector<type_code_t>& type_codes,
                           std::shared_ptr<Array>* out) {
    return MakeSparse(type_ids, children, type_codes).Value(out);
  }

  ARROW_DEPRECATED("Use Result-returning version")
  static Status MakeSparse(const Array& type_ids,
                           const std::vector<std::shared_ptr<Array>>& children,
                           std::shared_ptr<Array>* out) {
    return MakeSparse(type_ids, children).Value(out);
  }

  /// Note that this buffer does not account for any slice offset
  std::shared_ptr<Buffer> type_codes() const { return data_->buffers[1]; }

  const type_code_t* raw_type_codes() const { return raw_type_codes_ + data_->offset; }

  /// The physical child id containing value at index.
  int child_id(int64_t i) const {
    return union_type_->child_ids()[raw_type_codes_[i + data_->offset]];
  }

  /// For dense arrays only.
  /// Note that this buffer does not account for any slice offset
  std::shared_ptr<Buffer> value_offsets() const { return data_->buffers[2]; }

  /// For dense arrays only.
  int32_t value_offset(int64_t i) const { return raw_value_offsets_[i + data_->offset]; }

  /// For dense arrays only.
  const int32_t* raw_value_offsets() const { return raw_value_offsets_ + data_->offset; }

  const UnionType* union_type() const { return union_type_; }

  UnionMode::type mode() const { return union_type_->mode(); }

  // Return the given field as an individual array.
  // For sparse unions, the returned array has its offset, length and null
  // count adjusted.
  // For dense unions, the returned array is unchanged.
  std::shared_ptr<Array> child(int pos) const;

 protected:
  void SetData(const std::shared_ptr<ArrayData>& data);

  const type_code_t* raw_type_codes_;
  const int32_t* raw_value_offsets_;
  const UnionType* union_type_;

  // For caching boxed child data
  mutable std::vector<std::shared_ptr<Array>> boxed_fields_;
};

// ----------------------------------------------------------------------
// DictionaryArray

/// \brief Array type for dictionary-encoded data with a
/// data-dependent dictionary
///
/// A dictionary array contains an array of non-negative integers (the
/// "dictionary indices") along with a data type containing a "dictionary"
/// corresponding to the distinct values represented in the data.
///
/// For example, the array
///
///   ["foo", "bar", "foo", "bar", "foo", "bar"]
///
/// with dictionary ["bar", "foo"], would have dictionary array representation
///
///   indices: [1, 0, 1, 0, 1, 0]
///   dictionary: ["bar", "foo"]
///
/// The indices in principle may have any integer type (signed or unsigned),
/// though presently data in IPC exchanges must be signed int32.
class ARROW_EXPORT DictionaryArray : public Array {
 public:
  using TypeClass = DictionaryType;

  explicit DictionaryArray(const std::shared_ptr<ArrayData>& data);

  DictionaryArray(const std::shared_ptr<DataType>& type,
                  const std::shared_ptr<Array>& indices,
                  const std::shared_ptr<Array>& dictionary);

  /// \brief Construct DictionaryArray from dictionary and indices
  /// array and validate
  ///
  /// This function does the validation of the indices and input type. It checks if
  /// all indices are non-negative and smaller than the size of the dictionary
  ///
  /// \param[in] type a dictionary type
  /// \param[in] dictionary the dictionary with same value type as the
  /// type object
  /// \param[in] indices an array of non-negative signed
  /// integers smaller than the size of the dictionary
  static Result<std::shared_ptr<Array>> FromArrays(
      const std::shared_ptr<DataType>& type, const std::shared_ptr<Array>& indices,
      const std::shared_ptr<Array>& dictionary);

  ARROW_DEPRECATED("Use Result-returning version")
  static Status FromArrays(const std::shared_ptr<DataType>& type,
                           const std::shared_ptr<Array>& indices,
                           const std::shared_ptr<Array>& dictionary,
                           std::shared_ptr<Array>* out);

  /// \brief Transpose this DictionaryArray
  ///
  /// This method constructs a new dictionary array with the given dictionary type,
  /// transposing indices using the transpose map.
  /// The type and the transpose map are typically computed using
  /// DictionaryUnifier.
  ///
  /// \param[in] type the new type object
  /// \param[in] dictionary the new dictionary
  /// \param[in] transpose_map transposition array of this array's indices
  ///   into the target array's indices
  /// \param[in] pool a pool to allocate the array data from
  Result<std::shared_ptr<Array>> Transpose(
      const std::shared_ptr<DataType>& type, const std::shared_ptr<Array>& dictionary,
      const int32_t* transpose_map, MemoryPool* pool = default_memory_pool()) const;

  ARROW_DEPRECATED("Use Result-returning version")
  Status Transpose(MemoryPool* pool, const std::shared_ptr<DataType>& type,
                   const std::shared_ptr<Array>& dictionary, const int32_t* transpose_map,
                   std::shared_ptr<Array>* out) const;

  /// \brief Determine whether dictionary arrays may be compared without unification
  bool CanCompareIndices(const DictionaryArray& other) const;

  /// \brief Return the dictionary for this array, which is stored as
  /// a member of the ArrayData internal structure
  std::shared_ptr<Array> dictionary() const;
  std::shared_ptr<Array> indices() const;

  const DictionaryType* dict_type() const { return dict_type_; }

 private:
  void SetData(const std::shared_ptr<ArrayData>& data);
  const DictionaryType* dict_type_;
  std::shared_ptr<Array> indices_;
};

}  // namespace arrow
