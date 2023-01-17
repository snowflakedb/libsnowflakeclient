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

#include "arrow/python/python_to_arrow.h"
#include "arrow/python/numpy_interop.h"

#include <datetime.h>

#include <algorithm>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "arrow/array.h"
#include "arrow/builder.h"
#include "arrow/status.h"
#include "arrow/table.h"
#include "arrow/type.h"
#include "arrow/type_traits.h"
#include "arrow/util/checked_cast.h"
#include "arrow/util/decimal.h"
#include "arrow/util/logging.h"

#include "arrow/python/datetime.h"
#include "arrow/python/decimal.h"
#include "arrow/python/helpers.h"
#include "arrow/python/inference.h"
#include "arrow/python/iterators.h"
#include "arrow/python/numpy_convert.h"
#include "arrow/python/type_traits.h"

namespace arrow {

using internal::checked_cast;
using internal::checked_pointer_cast;

namespace py {

// ----------------------------------------------------------------------
// Sequence converter base and CRTP "middle" subclasses

class SeqConverter;

// Forward-declare converter factory
Status GetConverter(const std::shared_ptr<DataType>& type, bool from_pandas,
                    bool strict_conversions, std::unique_ptr<SeqConverter>* out);

// Marshal Python sequence (list, tuple, etc.) to Arrow array
class SeqConverter {
 public:
  virtual ~SeqConverter() = default;

  // Initialize the sequence converter with an ArrayBuilder created
  // externally. The reason for this interface is that we have
  // arrow::MakeBuilder which also creates child builders for nested types, so
  // we have to pass in the child builders to child SeqConverter in the case of
  // converting Python objects to Arrow nested types
  virtual Status Init(ArrayBuilder* builder) = 0;

  // Append a single (non-sequence) Python datum to the underlying builder,
  // virtual function
  virtual Status AppendSingleVirtual(PyObject* obj) = 0;

  // Append the contents of a Python sequence to the underlying builder,
  // virtual version
  virtual Status AppendMultiple(PyObject* seq, int64_t size) = 0;

  // Append the contents of a Python sequence to the underlying builder,
  // virtual version
  virtual Status AppendMultipleMasked(PyObject* seq, PyObject* mask, int64_t size) = 0;

  virtual Status Close() {
    if (chunks_.size() == 0 || builder_->length() > 0) {
      std::shared_ptr<Array> last_chunk;
      RETURN_NOT_OK(builder_->Finish(&last_chunk));
      chunks_.emplace_back(std::move(last_chunk));
    }
    return Status::OK();
  }

  virtual Status GetResult(std::shared_ptr<ChunkedArray>* out) {
    // Still some accumulated data in the builder. If there are no chunks, we
    // always call Finish to deal with the edge case where a size-0 sequence
    // was converted with a specific output type, like array([], type=t)
    RETURN_NOT_OK(Close());
    *out = std::make_shared<ChunkedArray>(chunks_, builder_->type());
    return Status::OK();
  }

  ArrayBuilder* builder() const { return builder_; }

  int num_chunks() const { return static_cast<int>(chunks_.size()); }

 protected:
  ArrayBuilder* builder_;
  bool unfinished_builder_;
  std::vector<std::shared_ptr<Array>> chunks_;
};

enum class NullCoding : char { NONE_ONLY, PANDAS_SENTINELS };

template <NullCoding kind>
struct NullChecker {};

template <>
struct NullChecker<NullCoding::NONE_ONLY> {
  static inline bool Check(PyObject* obj) { return obj == Py_None; }
};

template <>
struct NullChecker<NullCoding::PANDAS_SENTINELS> {
  static inline bool Check(PyObject* obj) { return internal::PandasObjectIsNull(obj); }
};

// ----------------------------------------------------------------------
// Helper templates to append PyObject* to builder for each target conversion
// type

template <typename Type, typename Enable = void>
struct Unbox {};

template <typename Type>
struct Unbox<Type, enable_if_integer<Type>> {
  using BuilderType = typename TypeTraits<Type>::BuilderType;
  static inline Status Append(BuilderType* builder, PyObject* obj) {
    typename Type::c_type value;
    RETURN_NOT_OK(internal::CIntFromPython(obj, &value));
    return builder->Append(value);
  }
};

template <>
struct Unbox<HalfFloatType> {
  static inline Status Append(HalfFloatBuilder* builder, PyObject* obj) {
    npy_half val;
    RETURN_NOT_OK(PyFloat_AsHalf(obj, &val));
    return builder->Append(val);
  }
};

template <>
struct Unbox<FloatType> {
  static inline Status Append(FloatBuilder* builder, PyObject* obj) {
    if (internal::PyFloatScalar_Check(obj)) {
      float val = static_cast<float>(PyFloat_AsDouble(obj));
      RETURN_IF_PYERROR();
      return builder->Append(val);
    } else if (internal::PyIntScalar_Check(obj)) {
      float val = 0;
      RETURN_NOT_OK(internal::IntegerScalarToFloat32Safe(obj, &val));
      return builder->Append(val);
    } else {
      return internal::InvalidValue(obj, "tried to convert to float32");
    }
  }
};

template <>
struct Unbox<DoubleType> {
  static inline Status Append(DoubleBuilder* builder, PyObject* obj) {
    if (PyFloat_Check(obj)) {
      double val = PyFloat_AS_DOUBLE(obj);
      return builder->Append(val);
    } else if (internal::PyFloatScalar_Check(obj)) {
      // Other kinds of float-y things
      double val = PyFloat_AsDouble(obj);
      RETURN_IF_PYERROR();
      return builder->Append(val);
    } else if (internal::PyIntScalar_Check(obj)) {
      double val = 0;
      RETURN_NOT_OK(internal::IntegerScalarToDoubleSafe(obj, &val));
      return builder->Append(val);
    } else {
      return internal::InvalidValue(obj, "tried to convert to double");
    }
  }
};

// We use CRTP to avoid virtual calls to the AppendItem(), AppendNull(), and
// IsNull() on the hot path
template <typename Type, class Derived, NullCoding null_coding = NullCoding::NONE_ONLY>
class TypedConverter : public SeqConverter {
 public:
  using BuilderType = typename TypeTraits<Type>::BuilderType;

  Status Init(ArrayBuilder* builder) override {
    builder_ = builder;
    DCHECK_NE(builder_, nullptr);
    typed_builder_ = checked_cast<BuilderType*>(builder);
    return Status::OK();
  }

  bool CheckNull(PyObject* obj) const { return NullChecker<null_coding>::Check(obj); }

  // Append a missing item (default implementation)
  Status AppendNull() { return this->typed_builder_->AppendNull(); }

  // This is overridden in several subclasses, but if an Unbox implementation
  // is defined, it will be used here
  Status AppendItem(PyObject* obj) { return Unbox<Type>::Append(typed_builder_, obj); }

  Status AppendSingle(PyObject* obj) {
    auto self = checked_cast<Derived*>(this);
    return CheckNull(obj) ? self->AppendNull() : self->AppendItem(obj);
  }

  Status AppendSingleVirtual(PyObject* obj) override { return AppendSingle(obj); }

  Status AppendMultiple(PyObject* obj, int64_t size) override {
    /// Ensure we've allocated enough space
    RETURN_NOT_OK(this->typed_builder_->Reserve(size));
    // Iterate over the items adding each one
    auto self = checked_cast<Derived*>(this);
    return internal::VisitSequence(obj, [self](PyObject* item, bool* /* unused */) {
      return self->AppendSingle(item);
    });
  }

  Status AppendMultipleMasked(PyObject* obj, PyObject* mask, int64_t size) override {
    /// Ensure we've allocated enough space
    RETURN_NOT_OK(this->typed_builder_->Reserve(size));
    // Iterate over the items adding each one
    auto self = checked_cast<Derived*>(this);
    return internal::VisitSequenceMasked(
        obj, mask, [self](PyObject* item, bool is_masked, bool* /* unused */) {
          if (is_masked) {
            return self->AppendNull();
          } else {
            // This will also apply the null-checking convention in the event
            // that the value is not masked
            return self->AppendSingle(item);
          }
        });
  }

 protected:
  BuilderType* typed_builder_;
};

// ----------------------------------------------------------------------
// Sequence converter for null type

template <NullCoding null_coding>
class NullConverter
    : public TypedConverter<NullType, NullConverter<null_coding>, null_coding> {
 public:
  Status AppendItem(PyObject* obj) {
    return internal::InvalidValue(obj, "converting to null type");
  }
};

// ----------------------------------------------------------------------
// Sequence converter for boolean type

template <NullCoding null_coding>
class BoolConverter
    : public TypedConverter<BooleanType, BoolConverter<null_coding>, null_coding> {
 public:
  Status AppendItem(PyObject* obj) {
    if (obj == Py_True) {
      return this->typed_builder_->Append(true);
    } else if (obj == Py_False) {
      return this->typed_builder_->Append(false);
    } else {
      return internal::InvalidValue(obj, "tried to convert to boolean");
    }
  }
};

// ----------------------------------------------------------------------
// Sequence converter template for numeric (integer and floating point) types

template <typename Type, NullCoding null_coding>
class NumericConverter
    : public TypedConverter<Type, NumericConverter<Type, null_coding>, null_coding> {};

// ----------------------------------------------------------------------
// Sequence converters for temporal types

template <NullCoding null_coding>
class Date32Converter
    : public TypedConverter<Date32Type, Date32Converter<null_coding>, null_coding> {
 public:
  Status AppendItem(PyObject* obj) {
    int32_t t;
    if (PyDate_Check(obj)) {
      auto pydate = reinterpret_cast<PyDateTime_Date*>(obj);
      t = static_cast<int32_t>(internal::PyDate_to_days(pydate));
    } else {
      RETURN_NOT_OK(internal::CIntFromPython(obj, &t, "Integer too large for date32"));
    }
    return this->typed_builder_->Append(t);
  }
};

template <NullCoding null_coding>
class Date64Converter
    : public TypedConverter<Date64Type, Date64Converter<null_coding>, null_coding> {
 public:
  Status AppendItem(PyObject* obj) {
    int64_t t;
    if (PyDateTime_Check(obj)) {
      auto pydate = reinterpret_cast<PyDateTime_DateTime*>(obj);
      t = internal::PyDateTime_to_ms(pydate);
      // Truncate any intraday milliseconds
      t -= t % 86400000LL;
    } else if (PyDate_Check(obj)) {
      auto pydate = reinterpret_cast<PyDateTime_Date*>(obj);
      t = internal::PyDate_to_ms(pydate);
    } else {
      RETURN_NOT_OK(internal::CIntFromPython(obj, &t, "Integer too large for date64"));
    }
    return this->typed_builder_->Append(t);
  }
};

template <NullCoding null_coding>
class Time32Converter
    : public TypedConverter<Time32Type, Time32Converter<null_coding>, null_coding> {
 public:
  explicit Time32Converter(TimeUnit::type unit) : unit_(unit) {}

  Status AppendItem(PyObject* obj) {
    // TODO(kszucs): option for strict conversion?
    int32_t t;
    if (PyTime_Check(obj)) {
      // datetime.time stores microsecond resolution
      switch (unit_) {
        case TimeUnit::SECOND:
          t = static_cast<int32_t>(internal::PyTime_to_s(obj));
          break;
        case TimeUnit::MILLI:
          t = static_cast<int32_t>(internal::PyTime_to_ms(obj));
          break;
        default:
          return Status::UnknownError("Invalid time unit");
      }
    } else {
      RETURN_NOT_OK(internal::CIntFromPython(obj, &t, "Integer too large for int32"));
    }
    return this->typed_builder_->Append(t);
  }

 private:
  TimeUnit::type unit_;
};

template <NullCoding null_coding>
class Time64Converter
    : public TypedConverter<Time64Type, Time64Converter<null_coding>, null_coding> {
 public:
  explicit Time64Converter(TimeUnit::type unit) : unit_(unit) {}

  Status AppendItem(PyObject* obj) {
    int64_t t;
    if (PyTime_Check(obj)) {
      // datetime.time stores microsecond resolution
      switch (unit_) {
        case TimeUnit::MICRO:
          t = internal::PyTime_to_us(obj);
          break;
        case TimeUnit::NANO:
          t = internal::PyTime_to_ns(obj);
          break;
        default:
          return Status::UnknownError("Invalid time unit");
      }
    } else {
      RETURN_NOT_OK(internal::CIntFromPython(obj, &t, "Integer too large for int64"));
    }
    return this->typed_builder_->Append(t);
  }

 private:
  TimeUnit::type unit_;
};

template <typename ArrowType>
struct PyDateTimeTraits {};

template <>
struct PyDateTimeTraits<TimestampType> {
  static inline int PyTypeCheck(PyObject* obj) { return PyDateTime_Check(obj); }
  using PyDateTimeObject = PyDateTime_DateTime;

  static inline int64_t py_to_s(PyDateTime_DateTime* pydatetime) {
    return internal::PyDateTime_to_s(pydatetime);
  }
  static inline int64_t py_to_ms(PyDateTime_DateTime* pydatetime) {
    return internal::PyDateTime_to_ms(pydatetime);
  }
  static inline int64_t py_to_us(PyDateTime_DateTime* pydatetime) {
    return internal::PyDateTime_to_us(pydatetime);
  }
  static inline int64_t py_to_ns(PyDateTime_DateTime* pydatetime) {
    return internal::PyDateTime_to_ns(pydatetime);
  }

  using np_traits = internal::npy_traits<NPY_DATETIME>;
  using NpScalarObject = PyDatetimeScalarObject;
  static constexpr const char* const np_name = "datetime64";
};

template <>
struct PyDateTimeTraits<DurationType> {
  static inline int PyTypeCheck(PyObject* obj) { return PyDelta_Check(obj); }
  using PyDateTimeObject = PyDateTime_Delta;

  static inline int64_t py_to_s(PyDateTime_Delta* pytimedelta) {
    return internal::PyDelta_to_s(pytimedelta);
  }
  static inline int64_t py_to_ms(PyDateTime_Delta* pytimedelta) {
    return internal::PyDelta_to_ms(pytimedelta);
  }
  static inline int64_t py_to_us(PyDateTime_Delta* pytimedelta) {
    return internal::PyDelta_to_us(pytimedelta);
  }
  static inline int64_t py_to_ns(PyDateTime_Delta* pytimedelta) {
    return internal::PyDelta_to_ns(pytimedelta);
  }

  using np_traits = internal::npy_traits<NPY_TIMEDELTA>;
  using NpScalarObject = PyTimedeltaScalarObject;
  static constexpr const char* const np_name = "timedelta64";
};

template <NullCoding null_coding, typename ArrowType>
class TemporalConverter
    : public TypedConverter<ArrowType, TemporalConverter<null_coding, ArrowType>,
                            null_coding> {
 public:
  explicit TemporalConverter(TimeUnit::type unit) : unit_(unit) {}
  using traits = PyDateTimeTraits<ArrowType>;

  Status AppendItem(PyObject* obj) {
    int64_t t;
    if (traits::PyTypeCheck(obj)) {
      auto pydatetime = reinterpret_cast<typename traits::PyDateTimeObject*>(obj);

      switch (unit_) {
        case TimeUnit::SECOND:
          t = traits::py_to_s(pydatetime);
          break;
        case TimeUnit::MILLI:
          t = traits::py_to_ms(pydatetime);
          break;
        case TimeUnit::MICRO:
          t = traits::py_to_us(pydatetime);
          break;
        case TimeUnit::NANO:
          t = traits::py_to_ns(pydatetime);
          break;
        default:
          return Status::UnknownError("Invalid time unit");
      }
    } else if (PyArray_CheckAnyScalarExact(obj)) {
      // numpy.datetime64
      using npy_traits = typename traits::np_traits;
      static constexpr const char* const np_name = traits::np_name;

      std::shared_ptr<DataType> type;
      RETURN_NOT_OK(NumPyDtypeToArrow(PyArray_DescrFromScalar(obj), &type));
      if (type->id() != ArrowType::type_id) {
        return Status::Invalid("Expected np.", np_name, " but got: ", type->ToString());
      }
      const ArrowType& ttype = checked_cast<const ArrowType&>(*type);
      if (unit_ != ttype.unit()) {
        return Status::NotImplemented("Cannot convert NumPy ", np_name,
                                      " objects with differing unit");
      }

      t = reinterpret_cast<typename traits::NpScalarObject*>(obj)->obval;
      if (npy_traits::isnull(t)) {
        // checks numpy NaT sentinel after conversion
        return this->typed_builder_->AppendNull();
      }
    } else {
      RETURN_NOT_OK(internal::CIntFromPython(obj, &t));
    }
    return this->typed_builder_->Append(t);
  }

 private:
  TimeUnit::type unit_;
};

// ----------------------------------------------------------------------
// Sequence converters for Binary, FixedSizeBinary, String

namespace detail {

template <typename BuilderType>
inline Status AppendPyString(BuilderType* builder, const PyBytesView& view,
                             bool* is_full) {
  if (view.size > BuilderType::memory_limit()) {
    return Status::Invalid("string too large for datatype");
  }
  DCHECK_GE(view.size, 0);
  // Did we reach the builder size limit?
  if (ARROW_PREDICT_FALSE(builder->value_data_length() + view.size >
                          BuilderType::memory_limit())) {
    *is_full = true;
    return Status::OK();
  }
  RETURN_NOT_OK(builder->Append(::arrow::util::string_view(view.bytes, view.size)));
  *is_full = false;
  return Status::OK();
}

inline Status BuilderAppend(BinaryBuilder* builder, PyObject* obj, bool* is_full) {
  PyBytesView view;
  RETURN_NOT_OK(view.FromString(obj));
  return AppendPyString(builder, view, is_full);
}

inline Status BuilderAppend(LargeBinaryBuilder* builder, PyObject* obj, bool* is_full) {
  PyBytesView view;
  RETURN_NOT_OK(view.FromString(obj));
  return AppendPyString(builder, view, is_full);
}

inline Status BuilderAppend(FixedSizeBinaryBuilder* builder, PyObject* obj,
                            bool* is_full) {
  PyBytesView view;
  RETURN_NOT_OK(view.FromString(obj));
  const auto expected_length =
      checked_cast<const FixedSizeBinaryType&>(*builder->type()).byte_width();
  if (ARROW_PREDICT_FALSE(view.size != expected_length)) {
    std::stringstream ss;
    ss << "expected to be length " << expected_length << " was " << view.size;
    return internal::InvalidValue(obj, ss.str());
  }

  return AppendPyString(builder, view, is_full);
}

}  // namespace detail

template <typename Type, NullCoding null_coding>
class BinaryLikeConverter
    : public TypedConverter<Type, BinaryLikeConverter<Type, null_coding>, null_coding> {
 public:
  Status AppendItem(PyObject* obj) {
    // Accessing members of the templated base requires using this-> here
    bool is_full = false;
    RETURN_NOT_OK(detail::BuilderAppend(this->typed_builder_, obj, &is_full));

    // Exceeded capacity of builder
    if (ARROW_PREDICT_FALSE(is_full)) {
      std::shared_ptr<Array> chunk;
      RETURN_NOT_OK(this->typed_builder_->Finish(&chunk));
      this->chunks_.emplace_back(std::move(chunk));

      // Append the item now that the builder has been reset
      return detail::BuilderAppend(this->typed_builder_, obj, &is_full);
    }
    return Status::OK();
  }
};

template <NullCoding null_coding>
class BytesConverter : public BinaryLikeConverter<BinaryType, null_coding> {};

template <NullCoding null_coding>
class LargeBytesConverter : public BinaryLikeConverter<LargeBinaryType, null_coding> {};

template <NullCoding null_coding>
class FixedWidthBytesConverter
    : public BinaryLikeConverter<FixedSizeBinaryType, null_coding> {};

// For String/UTF8, if strict_conversions enabled, we reject any non-UTF8,
// otherwise we allow but return results as BinaryArray
template <typename TypeClass, bool STRICT, NullCoding null_coding>
class StringConverter
    : public TypedConverter<TypeClass, StringConverter<TypeClass, STRICT, null_coding>,
                            null_coding> {
 public:
  StringConverter() : binary_count_(0) {}

  Status Append(PyObject* obj, bool* is_full) {
    if (STRICT) {
      // Force output to be unicode / utf8 and validate that any binary values
      // are utf8
      bool is_utf8 = false;
      RETURN_NOT_OK(string_view_.FromString(obj, &is_utf8));
      if (!is_utf8) {
        return internal::InvalidValue(obj, "was not a utf8 string");
      }
    } else {
      // Non-strict conversion; keep track of whether values are unicode or
      // bytes; if any bytes are observe, the result will be bytes
      if (PyUnicode_Check(obj)) {
        RETURN_NOT_OK(string_view_.FromUnicode(obj));
      } else {
        // If not unicode or bytes, FromBinary will error
        RETURN_NOT_OK(string_view_.FromBinary(obj));
        ++binary_count_;
      }
    }

    return detail::AppendPyString(this->typed_builder_, string_view_, is_full);
  }

  Status AppendItem(PyObject* obj) {
    bool is_full = false;
    RETURN_NOT_OK(Append(obj, &is_full));

    // Exceeded capacity of builder
    if (ARROW_PREDICT_FALSE(is_full)) {
      std::shared_ptr<Array> chunk;
      RETURN_NOT_OK(this->typed_builder_->Finish(&chunk));
      this->chunks_.emplace_back(std::move(chunk));

      // Append the item now that the builder has been reset
      RETURN_NOT_OK(Append(obj, &is_full));
    }
    return Status::OK();
  }

  virtual Status GetResult(std::shared_ptr<ChunkedArray>* out) {
    RETURN_NOT_OK(SeqConverter::GetResult(out));

    // If we saw any non-unicode, cast results to BinaryArray
    if (binary_count_) {
      // We should have bailed out earlier
      DCHECK(!STRICT);

      auto binary_type =
          TypeTraits<typename TypeClass::EquivalentBinaryType>::type_singleton();
      return (*out)->View(binary_type).Value(out);
    }
    return Status::OK();
  }

 private:
  // Create a single instance of PyBytesView here to prevent unnecessary object
  // creation/destruction
  PyBytesView string_view_;

  int64_t binary_count_;
};

// ----------------------------------------------------------------------
// Convert lists (NumPy arrays containing lists or ndarrays as values)

// If the value type does not match the expected NumPy dtype, then fall through
// to a slower PySequence-based path
#define LIST_FAST_CASE(TYPE, NUMPY_TYPE, ArrowType)               \
  case Type::TYPE: {                                              \
    if (PyArray_DESCR(arr)->type_num != NUMPY_TYPE) {             \
      return value_converter_->AppendMultiple(obj, value_length); \
    }                                                             \
    return AppendNdarrayTypedItem<NUMPY_TYPE, ArrowType>(arr);    \
  }

// Use internal::VisitSequence, fast for NPY_OBJECT but slower otherwise
#define LIST_SLOW_CASE(TYPE)                                    \
  case Type::TYPE: {                                            \
    return value_converter_->AppendMultiple(obj, value_length); \
  }

// Base class for ListConverter and FixedSizeListConverter (to have both work with CRTP)
template <typename TypeClass, class Derived, NullCoding null_coding>
class BaseListConverter : public TypedConverter<TypeClass, Derived, null_coding> {
 public:
  using BuilderType = typename TypeTraits<TypeClass>::BuilderType;

  explicit BaseListConverter(bool from_pandas, bool strict_conversions)
      : from_pandas_(from_pandas), strict_conversions_(strict_conversions) {}

  Status Init(ArrayBuilder* builder) {
    this->builder_ = builder;
    this->typed_builder_ = checked_cast<BuilderType*>(builder);

    value_type_ = checked_cast<const TypeClass&>(*builder->type()).value_type();
    RETURN_NOT_OK(
        GetConverter(value_type_, from_pandas_, strict_conversions_, &value_converter_));
    return value_converter_->Init(this->typed_builder_->value_builder());
  }

  template <int NUMPY_TYPE, typename Type>
  Status AppendNdarrayTypedItem(PyArrayObject* arr) {
    using traits = internal::npy_traits<NUMPY_TYPE>;
    using T = typename traits::value_type;
    using ValueBuilderType = typename TypeTraits<Type>::BuilderType;

    const bool null_sentinels_possible =
        // Always treat Numpy's NaT as null
        NUMPY_TYPE == NPY_DATETIME || NUMPY_TYPE == NPY_TIMEDELTA ||
        // Observing pandas's null sentinels
        (from_pandas_ && traits::supports_nulls);

    auto child_builder = checked_cast<ValueBuilderType*>(value_converter_->builder());

    // TODO(wesm): Vector append when not strided
    Ndarray1DIndexer<T> values(arr);
    if (null_sentinels_possible) {
      for (int64_t i = 0; i < values.size(); ++i) {
        if (traits::isnull(values[i])) {
          RETURN_NOT_OK(child_builder->AppendNull());
        } else {
          RETURN_NOT_OK(child_builder->Append(values[i]));
        }
      }
    } else {
      for (int64_t i = 0; i < values.size(); ++i) {
        RETURN_NOT_OK(child_builder->Append(values[i]));
      }
    }
    return Status::OK();
  }

  Status AppendNdarrayItem(PyObject* obj) {
    PyArrayObject* arr = reinterpret_cast<PyArrayObject*>(obj);

    if (PyArray_NDIM(arr) != 1) {
      return Status::Invalid("Can only convert 1-dimensional array values");
    }

    const int64_t value_length = PyArray_SIZE(arr);

    switch (value_type_->id()) {
      LIST_SLOW_CASE(NA)
      LIST_FAST_CASE(UINT8, NPY_UINT8, UInt8Type)
      LIST_FAST_CASE(INT8, NPY_INT8, Int8Type)
      LIST_FAST_CASE(UINT16, NPY_UINT16, UInt16Type)
      LIST_FAST_CASE(INT16, NPY_INT16, Int16Type)
      LIST_FAST_CASE(UINT32, NPY_UINT32, UInt32Type)
      LIST_FAST_CASE(INT32, NPY_INT32, Int32Type)
      LIST_FAST_CASE(UINT64, NPY_UINT64, UInt64Type)
      LIST_FAST_CASE(INT64, NPY_INT64, Int64Type)
      LIST_SLOW_CASE(DATE32)
      LIST_SLOW_CASE(DATE64)
      LIST_SLOW_CASE(TIME32)
      LIST_SLOW_CASE(TIME64)
      LIST_FAST_CASE(TIMESTAMP, NPY_DATETIME, TimestampType)
      LIST_FAST_CASE(DURATION, NPY_TIMEDELTA, DurationType)
      LIST_FAST_CASE(HALF_FLOAT, NPY_FLOAT16, HalfFloatType)
      LIST_FAST_CASE(FLOAT, NPY_FLOAT, FloatType)
      LIST_FAST_CASE(DOUBLE, NPY_DOUBLE, DoubleType)
      LIST_SLOW_CASE(BINARY)
      LIST_SLOW_CASE(FIXED_SIZE_BINARY)
      LIST_SLOW_CASE(STRING)
      case Type::LIST: {
        if (PyArray_DESCR(arr)->type_num != NPY_OBJECT) {
          return Status::Invalid(
              "Can only convert list types from NumPy object "
              "array input");
        }
        return internal::VisitSequence(obj, [this](PyObject* item, bool*) {
          return value_converter_->AppendSingleVirtual(item);
        });
      }
      default: {
        return Status::TypeError("Unknown list item type: ", value_type_->ToString());
      }
    }
  }

  Status AppendItem(PyObject* obj) {
    RETURN_NOT_OK(this->typed_builder_->Append());
    if (PyArray_Check(obj)) {
      return AppendNdarrayItem(obj);
    }
    if (!PySequence_Check(obj)) {
      return internal::InvalidType(obj,
                                   "was not a sequence or recognized null"
                                   " for conversion to list type");
    }
    int64_t list_size = static_cast<int64_t>(PySequence_Size(obj));
    return value_converter_->AppendMultiple(obj, list_size);
  }

  virtual Status GetResult(std::shared_ptr<ChunkedArray>* out) {
    // TODO: Improved handling of chunked children
    if (value_converter_->num_chunks() > 0) {
      return Status::Invalid("List child type ",
                             value_converter_->builder()->type()->ToString(),
                             " overflowed the capacity of a single chunk");
    }
    return SeqConverter::GetResult(out);
  }

 protected:
  std::shared_ptr<DataType> value_type_;
  std::unique_ptr<SeqConverter> value_converter_;
  bool from_pandas_;
  bool strict_conversions_;
};

template <typename TypeClass, NullCoding null_coding>
class ListConverter
    : public BaseListConverter<TypeClass, ListConverter<TypeClass, null_coding>,
                               null_coding> {
 public:
  using BASE =
      BaseListConverter<TypeClass, ListConverter<TypeClass, null_coding>, null_coding>;
  using BASE::BASE;
};

template <NullCoding null_coding>
class FixedSizeListConverter
    : public BaseListConverter<FixedSizeListType, FixedSizeListConverter<null_coding>,
                               null_coding> {
 public:
  using BASE = BaseListConverter<FixedSizeListType, FixedSizeListConverter<null_coding>,
                                 null_coding>;
  using BASE::BASE;

  Status Init(ArrayBuilder* builder) {
    RETURN_NOT_OK(BASE::Init(builder));
    list_size_ = checked_pointer_cast<FixedSizeListType>(builder->type())->list_size();
    return Status::OK();
  }

  Status AppendItem(PyObject* obj) {
    // the same as BaseListConverter but with additional length checks
    RETURN_NOT_OK(this->typed_builder_->Append());
    if (PyArray_Check(obj)) {
      int64_t list_size = static_cast<int64_t>(PyArray_Size(obj));
      if (list_size != list_size_) {
        return Status::Invalid("Length of item not correct: expected ", list_size_,
                               " but got array of size ", list_size);
      }
      return this->AppendNdarrayItem(obj);
    }
    if (!PySequence_Check(obj)) {
      return internal::InvalidType(obj,
                                   "was not a sequence or recognized null"
                                   " for conversion to list type");
    }
    int64_t list_size = static_cast<int64_t>(PySequence_Size(obj));
    if (list_size != list_size_) {
      return Status::Invalid("Length of item not correct: expected ", list_size_,
                             " but got list of size ", list_size);
    }
    return this->value_converter_->AppendMultiple(obj, list_size);
  }

 protected:
  int64_t list_size_;
};

// ----------------------------------------------------------------------
// Convert maps

// Define a MapConverter as a ListConverter that uses MapBuilder.value_builder
// to append struct of key/value pairs
template <NullCoding null_coding>
class MapConverter
    : public BaseListConverter<MapType, MapConverter<null_coding>, null_coding> {
 public:
  using BASE = BaseListConverter<MapType, MapConverter<null_coding>, null_coding>;

  explicit MapConverter(bool from_pandas, bool strict_conversions)
      : BASE(from_pandas, strict_conversions), key_builder_(nullptr) {}

  Status AppendSingleVirtual(PyObject* obj) override {
    RETURN_NOT_OK(BASE::AppendSingleVirtual(obj));
    return VerifyLastStructAppended();
  }

  Status AppendMultiple(PyObject* seq, int64_t size) override {
    RETURN_NOT_OK(BASE::AppendMultiple(seq, size));
    return VerifyLastStructAppended();
  }

  Status AppendMultipleMasked(PyObject* seq, PyObject* mask, int64_t size) override {
    RETURN_NOT_OK(BASE::AppendMultipleMasked(seq, mask, size));
    return VerifyLastStructAppended();
  }

 protected:
  Status VerifyLastStructAppended() {
    // The struct_builder may not have field_builders initialized in constructor, so
    // assign key_builder lazily
    if (key_builder_ == nullptr) {
      auto struct_builder =
          checked_cast<StructBuilder*>(BASE::value_converter_->builder());
      key_builder_ = struct_builder->field_builder(0);
    }
    if (key_builder_->null_count() > 0) {
      return Status::Invalid("Invalid Map: key field can not contain null values");
    }
    return Status::OK();
  }

 private:
  ArrayBuilder* key_builder_;
};

// ----------------------------------------------------------------------
// Convert structs

template <NullCoding null_coding>
class StructConverter
    : public TypedConverter<StructType, StructConverter<null_coding>, null_coding> {
 public:
  explicit StructConverter(bool from_pandas, bool strict_conversions)
      : from_pandas_(from_pandas), strict_conversions_(strict_conversions) {}

  Status Init(ArrayBuilder* builder) {
    this->builder_ = builder;
    this->typed_builder_ = checked_cast<StructBuilder*>(builder);
    auto struct_type = checked_pointer_cast<StructType>(builder->type());

    num_fields_ = this->typed_builder_->num_fields();
    DCHECK_EQ(num_fields_, struct_type->num_children());

    field_name_bytes_list_.reset(PyList_New(num_fields_));
    field_name_unicode_list_.reset(PyList_New(num_fields_));
    RETURN_IF_PYERROR();

    // Initialize the child converters and field names
    for (int i = 0; i < num_fields_; i++) {
      const std::string& field_name(struct_type->child(i)->name());
      std::shared_ptr<DataType> field_type(struct_type->child(i)->type());

      std::unique_ptr<SeqConverter> value_converter;
      RETURN_NOT_OK(
          GetConverter(field_type, from_pandas_, strict_conversions_, &value_converter));
      RETURN_NOT_OK(value_converter->Init(this->typed_builder_->field_builder(i)));
      value_converters_.push_back(std::move(value_converter));

      // Store the field name as a PyObject, for dict matching
      PyObject* bytesobj =
          PyBytes_FromStringAndSize(field_name.c_str(), field_name.size());
      PyObject* unicodeobj =
          PyUnicode_FromStringAndSize(field_name.c_str(), field_name.size());
      RETURN_IF_PYERROR();
      PyList_SET_ITEM(field_name_bytes_list_.obj(), i, bytesobj);
      PyList_SET_ITEM(field_name_unicode_list_.obj(), i, unicodeobj);
    }

    return Status::OK();
  }

  Status AppendItem(PyObject* obj) {
    RETURN_NOT_OK(this->typed_builder_->Append());
    // Note heterogeneous sequences are not allowed
    if (ARROW_PREDICT_FALSE(source_kind_ == SourceKind::UNKNOWN)) {
      if (PyDict_Check(obj)) {
        source_kind_ = SourceKind::DICTS;
      } else if (PyTuple_Check(obj)) {
        source_kind_ = SourceKind::TUPLES;
      }
    }
    if (PyDict_Check(obj) && source_kind_ == SourceKind::DICTS) {
      return AppendDictItem(obj);
    } else if (PyTuple_Check(obj) && source_kind_ == SourceKind::TUPLES) {
      return AppendTupleItem(obj);
    } else {
      return internal::InvalidType(obj,
                                   "was not a dict, tuple, or recognized null value"
                                   " for conversion to struct type");
    }
  }

  // Append a missing item
  Status AppendNull() {
    RETURN_NOT_OK(this->typed_builder_->AppendNull());
    // Need to also insert a missing item on all child builders
    // (compare with ListConverter)
    for (int i = 0; i < num_fields_; i++) {
      RETURN_NOT_OK(value_converters_[i]->AppendSingleVirtual(Py_None));
    }
    return Status::OK();
  }

 protected:
  Status AppendDictItem(PyObject* obj) {
    if (dict_key_kind_ == DictKeyKind::UNICODE) {
      return AppendDictItemWithUnicodeKeys(obj);
    }
    if (dict_key_kind_ == DictKeyKind::BYTES) {
      return AppendDictItemWithBytesKeys(obj);
    }
    for (int i = 0; i < num_fields_; i++) {
      PyObject* nameobj = PyList_GET_ITEM(field_name_unicode_list_.obj(), i);
      PyObject* valueobj = PyDict_GetItem(obj, nameobj);
      if (valueobj != NULL) {
        dict_key_kind_ = DictKeyKind::UNICODE;
        return AppendDictItemWithUnicodeKeys(obj);
      }
      RETURN_IF_PYERROR();
      // Unicode key not present, perhaps bytes key is?
      nameobj = PyList_GET_ITEM(field_name_bytes_list_.obj(), i);
      valueobj = PyDict_GetItem(obj, nameobj);
      if (valueobj != NULL) {
        dict_key_kind_ = DictKeyKind::BYTES;
        return AppendDictItemWithBytesKeys(obj);
      }
      RETURN_IF_PYERROR();
    }
    // If we come here, it means all keys are absent
    for (int i = 0; i < num_fields_; i++) {
      RETURN_NOT_OK(value_converters_[i]->AppendSingleVirtual(Py_None));
    }
    return Status::OK();
  }

  Status AppendDictItemWithBytesKeys(PyObject* obj) {
    return AppendDictItem(obj, field_name_bytes_list_.obj());
  }

  Status AppendDictItemWithUnicodeKeys(PyObject* obj) {
    return AppendDictItem(obj, field_name_unicode_list_.obj());
  }

  Status AppendDictItem(PyObject* obj, PyObject* field_name_list) {
    // NOTE we're ignoring any extraneous dict items
    for (int i = 0; i < num_fields_; i++) {
      PyObject* nameobj = PyList_GET_ITEM(field_name_list, i);  // borrowed
      PyObject* valueobj = PyDict_GetItem(obj, nameobj);        // borrowed
      if (valueobj == NULL) {
        RETURN_IF_PYERROR();
      }
      RETURN_NOT_OK(
          value_converters_[i]->AppendSingleVirtual(valueobj ? valueobj : Py_None));
    }
    return Status::OK();
  }

  Status AppendTupleItem(PyObject* obj) {
    if (PyTuple_GET_SIZE(obj) != num_fields_) {
      return Status::Invalid("Tuple size must be equal to number of struct fields");
    }
    for (int i = 0; i < num_fields_; i++) {
      PyObject* valueobj = PyTuple_GET_ITEM(obj, i);
      RETURN_NOT_OK(value_converters_[i]->AppendSingleVirtual(valueobj));
    }
    return Status::OK();
  }

  std::vector<std::unique_ptr<SeqConverter>> value_converters_;
  OwnedRef field_name_unicode_list_;
  OwnedRef field_name_bytes_list_;
  int num_fields_;
  // Whether we're converting from a sequence of dicts or tuples
  enum class SourceKind { UNKNOWN, DICTS, TUPLES } source_kind_ = SourceKind::UNKNOWN;
  enum class DictKeyKind {
    UNKNOWN,
    BYTES,
    UNICODE
  } dict_key_kind_ = DictKeyKind::UNKNOWN;
  bool from_pandas_;
  bool strict_conversions_;
};

template <NullCoding null_coding>
class DecimalConverter
    : public TypedConverter<arrow::Decimal128Type, DecimalConverter<null_coding>,
                            null_coding> {
 public:
  using BASE =
      TypedConverter<arrow::Decimal128Type, DecimalConverter<null_coding>, null_coding>;

  Status Init(ArrayBuilder* builder) override {
    RETURN_NOT_OK(BASE::Init(builder));
    decimal_type_ = checked_pointer_cast<DecimalType>(this->typed_builder_->type());
    return Status::OK();
  }

  Status AppendItem(PyObject* obj) {
    Decimal128 value;
    RETURN_NOT_OK(internal::DecimalFromPyObject(obj, *decimal_type_, &value));
    return this->typed_builder_->Append(value);
  }

 private:
  std::shared_ptr<DecimalType> decimal_type_;
};

#define NUMERIC_CONVERTER(TYPE_ENUM, TYPE)                                         \
  case Type::TYPE_ENUM:                                                            \
    *out = std::unique_ptr<SeqConverter>(new NumericConverter<TYPE, null_coding>); \
    break;

#define SIMPLE_CONVERTER_CASE(TYPE_ENUM, TYPE_CLASS)                   \
  case Type::TYPE_ENUM:                                                \
    *out = std::unique_ptr<SeqConverter>(new TYPE_CLASS<null_coding>); \
    break;

// Dynamic constructor for sequence converters
template <NullCoding null_coding>
Status GetConverterFlat(const std::shared_ptr<DataType>& type, bool strict_conversions,
                        std::unique_ptr<SeqConverter>* out) {
  switch (type->id()) {
    SIMPLE_CONVERTER_CASE(NA, NullConverter);
    SIMPLE_CONVERTER_CASE(BOOL, BoolConverter);
    NUMERIC_CONVERTER(INT8, Int8Type);
    NUMERIC_CONVERTER(INT16, Int16Type);
    NUMERIC_CONVERTER(INT32, Int32Type);
    NUMERIC_CONVERTER(INT64, Int64Type);
    NUMERIC_CONVERTER(UINT8, UInt8Type);
    NUMERIC_CONVERTER(UINT16, UInt16Type);
    NUMERIC_CONVERTER(UINT32, UInt32Type);
    NUMERIC_CONVERTER(UINT64, UInt64Type);
    NUMERIC_CONVERTER(HALF_FLOAT, HalfFloatType);
    NUMERIC_CONVERTER(FLOAT, FloatType);
    NUMERIC_CONVERTER(DOUBLE, DoubleType);
    SIMPLE_CONVERTER_CASE(DECIMAL, DecimalConverter);
    SIMPLE_CONVERTER_CASE(BINARY, BytesConverter);
    SIMPLE_CONVERTER_CASE(LARGE_BINARY, LargeBytesConverter);
    SIMPLE_CONVERTER_CASE(FIXED_SIZE_BINARY, FixedWidthBytesConverter);
    SIMPLE_CONVERTER_CASE(DATE32, Date32Converter);
    SIMPLE_CONVERTER_CASE(DATE64, Date64Converter);
    case Type::STRING:
      if (strict_conversions) {
        *out = std::unique_ptr<SeqConverter>(
            new StringConverter<StringType, true, null_coding>());
      } else {
        *out = std::unique_ptr<SeqConverter>(
            new StringConverter<StringType, false, null_coding>());
      }
      break;
    case Type::LARGE_STRING:
      if (strict_conversions) {
        *out = std::unique_ptr<SeqConverter>(
            new StringConverter<LargeStringType, true, null_coding>());
      } else {
        *out = std::unique_ptr<SeqConverter>(
            new StringConverter<LargeStringType, false, null_coding>());
      }
      break;
    case Type::TIME32: {
      *out = std::unique_ptr<SeqConverter>(new Time32Converter<null_coding>(
          checked_cast<const Time32Type&>(*type).unit()));
      break;
    }
    case Type::TIME64: {
      *out = std::unique_ptr<SeqConverter>(new Time64Converter<null_coding>(
          checked_cast<const Time64Type&>(*type).unit()));
      break;
    }
    case Type::TIMESTAMP: {
      *out =
          std::unique_ptr<SeqConverter>(new TemporalConverter<null_coding, TimestampType>(
              checked_cast<const TimestampType&>(*type).unit()));
      break;
    }
    case Type::DURATION: {
      *out =
          std::unique_ptr<SeqConverter>(new TemporalConverter<null_coding, DurationType>(
              checked_cast<const DurationType&>(*type).unit()));
      break;
    }
    default:
      return Status::NotImplemented("Sequence converter for type ", type->ToString(),
                                    " not implemented");
  }
  return Status::OK();
}

Status GetConverter(const std::shared_ptr<DataType>& type, bool from_pandas,
                    bool strict_conversions, std::unique_ptr<SeqConverter>* out) {
  switch (type->id()) {
    case Type::LIST:
      if (from_pandas) {
        *out = std::unique_ptr<SeqConverter>(
            new ListConverter<ListType, NullCoding::PANDAS_SENTINELS>(
                from_pandas, strict_conversions));
      } else {
        *out = std::unique_ptr<SeqConverter>(
            new ListConverter<ListType, NullCoding::NONE_ONLY>(from_pandas,
                                                               strict_conversions));
      }
      return Status::OK();
    case Type::LARGE_LIST:
      if (from_pandas) {
        *out = std::unique_ptr<SeqConverter>(
            new ListConverter<LargeListType, NullCoding::PANDAS_SENTINELS>(
                from_pandas, strict_conversions));
      } else {
        *out = std::unique_ptr<SeqConverter>(
            new ListConverter<LargeListType, NullCoding::NONE_ONLY>(from_pandas,
                                                                    strict_conversions));
      }
      return Status::OK();
    case Type::MAP:
      if (from_pandas) {
        *out =
            std::unique_ptr<SeqConverter>(new MapConverter<NullCoding::PANDAS_SENTINELS>(
                from_pandas, strict_conversions));
      } else {
        *out = std::unique_ptr<SeqConverter>(
            new MapConverter<NullCoding::NONE_ONLY>(from_pandas, strict_conversions));
      }
      return Status::OK();
    case Type::FIXED_SIZE_LIST:
      if (from_pandas) {
        *out = std::unique_ptr<SeqConverter>(
            new FixedSizeListConverter<NullCoding::PANDAS_SENTINELS>(from_pandas,
                                                                     strict_conversions));
      } else {
        *out = std::unique_ptr<SeqConverter>(
            new FixedSizeListConverter<NullCoding::NONE_ONLY>(from_pandas,
                                                              strict_conversions));
      }
      return Status::OK();
    case Type::STRUCT:
      if (from_pandas) {
        *out = std::unique_ptr<SeqConverter>(
            new StructConverter<NullCoding::PANDAS_SENTINELS>(from_pandas,
                                                              strict_conversions));
      } else {
        *out = std::unique_ptr<SeqConverter>(
            new StructConverter<NullCoding::NONE_ONLY>(from_pandas, strict_conversions));
      }
      return Status::OK();
    default:
      break;
  }

  if (from_pandas) {
    RETURN_NOT_OK(
        GetConverterFlat<NullCoding::PANDAS_SENTINELS>(type, strict_conversions, out));
  } else {
    RETURN_NOT_OK(GetConverterFlat<NullCoding::NONE_ONLY>(type, strict_conversions, out));
  }
  return Status::OK();
}

// ----------------------------------------------------------------------

// Convert *obj* to a sequence if necessary
// Fill *size* to its length.  If >= 0 on entry, *size* is an upper size
// bound that may lead to truncation.
Status ConvertToSequenceAndInferSize(PyObject* obj, PyObject** seq, int64_t* size) {
  if (PySequence_Check(obj)) {
    // obj is already a sequence
    int64_t real_size = static_cast<int64_t>(PySequence_Size(obj));
    if (*size < 0) {
      *size = real_size;
    } else {
      *size = std::min(real_size, *size);
    }
    Py_INCREF(obj);
    *seq = obj;
  } else if (*size < 0) {
    // unknown size, exhaust iterator
    *seq = PySequence_List(obj);
    RETURN_IF_PYERROR();
    *size = static_cast<int64_t>(PyList_GET_SIZE(*seq));
  } else {
    // size is known but iterator could be infinite
    Py_ssize_t i, n = *size;
    PyObject* iter = PyObject_GetIter(obj);
    RETURN_IF_PYERROR();
    OwnedRef iter_ref(iter);
    PyObject* lst = PyList_New(n);
    RETURN_IF_PYERROR();
    for (i = 0; i < n; i++) {
      PyObject* item = PyIter_Next(iter);
      if (!item) break;
      PyList_SET_ITEM(lst, i, item);
    }
    // Shrink list if len(iterator) < size
    if (i < n && PyList_SetSlice(lst, i, n, NULL)) {
      Py_DECREF(lst);
      return Status::UnknownError("failed to resize list");
    }
    *seq = lst;
    *size = std::min<int64_t>(i, *size);
  }
  return Status::OK();
}

Status ConvertPySequence(PyObject* sequence_source, PyObject* mask,
                         const PyConversionOptions& options,
                         std::shared_ptr<ChunkedArray>* out) {
  PyAcquireGIL lock;

  PyObject* seq;
  OwnedRef tmp_seq_nanny;

  std::shared_ptr<DataType> real_type;

  int64_t size = options.size;
  RETURN_NOT_OK(ConvertToSequenceAndInferSize(sequence_source, &seq, &size));
  tmp_seq_nanny.reset(seq);

  // In some cases, type inference may be "loose", like strings. If the user
  // passed pa.string(), then we will error if we encounter any non-UTF8
  // value. If not, then we will allow the result to be a BinaryArray
  bool strict_conversions = false;

  if (options.type == nullptr) {
    RETURN_NOT_OK(InferArrowType(seq, mask, options.from_pandas, &real_type));
  } else {
    real_type = options.type;
    strict_conversions = true;
  }
  DCHECK_GE(size, 0);

  // Create the sequence converter, initialize with the builder
  std::unique_ptr<SeqConverter> converter;
  RETURN_NOT_OK(
      GetConverter(real_type, options.from_pandas, strict_conversions, &converter));

  // Create ArrayBuilder for type, then pass into the SeqConverter
  // instance. The reason this is created here rather than in GetConverter is
  // because of nested types (child SeqConverter objects need the child
  // builders created by MakeBuilder)
  std::unique_ptr<ArrayBuilder> type_builder;
  RETURN_NOT_OK(MakeBuilder(options.pool, real_type, &type_builder));
  RETURN_NOT_OK(converter->Init(type_builder.get()));

  // Convert values
  if (mask != nullptr && mask != Py_None) {
    RETURN_NOT_OK(converter->AppendMultipleMasked(seq, mask, size));
  } else {
    RETURN_NOT_OK(converter->AppendMultiple(seq, size));
  }

  // Retrieve result. Conversion may yield one or more array values
  return converter->GetResult(out);
}

Status ConvertPySequence(PyObject* obj, const PyConversionOptions& options,
                         std::shared_ptr<ChunkedArray>* out) {
  return ConvertPySequence(obj, nullptr, options, out);
}

}  // namespace py
}  // namespace arrow
