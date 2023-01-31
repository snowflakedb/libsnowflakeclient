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

#include "arrow/scalar.h"

#include <memory>
#include <string>
#include <utility>

#include "arrow/array.h"
#include "arrow/buffer.h"
#include "arrow/compare.h"
#include "arrow/type.h"
#include "arrow/util/checked_cast.h"
#include "arrow/util/decimal.h"
#include "arrow/util/formatting.h"
#include "arrow/util/hashing.h"
#include "arrow/util/logging.h"
#include "arrow/util/parsing.h"
#include "arrow/util/time.h"
#include "arrow/visitor_inline.h"

namespace arrow {

using internal::checked_cast;
using internal::checked_pointer_cast;

bool Scalar::Equals(const Scalar& other) const { return ScalarEquals(*this, other); }

struct ScalarHashImpl {
  static std::hash<std::string> string_hash;

  Status Visit(const NullScalar& s) { return Status::OK(); }

  template <typename T>
  Status Visit(const internal::PrimitiveScalar<T>& s) {
    return ValueHash(s);
  }

  Status Visit(const BaseBinaryScalar& s) { return BufferHash(*s.value); }

  template <typename T>
  Status Visit(const TemporalScalar<T>& s) {
    return ValueHash(s);
  }

  Status Visit(const DayTimeIntervalScalar& s) {
    return StdHash(s.value.days) & StdHash(s.value.days);
  }

  Status Visit(const Decimal128Scalar& s) {
    return StdHash(s.value.low_bits()) & StdHash(s.value.high_bits());
  }

  Status Visit(const BaseListScalar& s) { return ArrayHash(*s.value); }

  Status Visit(const StructScalar& s) {
    for (const auto& child : s.value) {
      AccumulateHashFrom(*child);
    }
    return Status::OK();
  }

  // TODO(bkietz) implement less wimpy hashing when these have ValueType
  Status Visit(const UnionScalar& s) { return Status::OK(); }
  Status Visit(const DictionaryScalar& s) { return Status::OK(); }
  Status Visit(const ExtensionScalar& s) { return Status::OK(); }

  template <typename T>
  Status StdHash(const T& t) {
    static std::hash<T> hash;
    hash_ ^= hash(t);
    return Status::OK();
  }

  template <typename S>
  Status ValueHash(const S& s) {
    return StdHash(s.value);
  }

  Status BufferHash(const Buffer& b) {
    hash_ ^= internal::ComputeStringHash<1>(b.data(), b.size());
    return Status::OK();
  }

  Status ArrayHash(const Array& a) { return ArrayHash(*a.data()); }

  Status ArrayHash(const ArrayData& a) {
    RETURN_NOT_OK(StdHash(a.length) & StdHash(a.GetNullCount()));
    if (a.buffers[0] != nullptr) {
      // We can't visit values without unboxing the whole array, so only hash
      // the null bitmap for now.
      RETURN_NOT_OK(BufferHash(*a.buffers[0]));
    }
    for (const auto& child : a.child_data) {
      RETURN_NOT_OK(ArrayHash(*child));
    }
    return Status::OK();
  }

  explicit ScalarHashImpl(const Scalar& scalar) { AccumulateHashFrom(scalar); }

  void AccumulateHashFrom(const Scalar& scalar) {
    DCHECK_OK(StdHash(scalar.type->fingerprint()));
    DCHECK_OK(VisitScalarInline(scalar, this));
  }

  size_t hash_ = 0;
};

size_t Scalar::Hash::hash(const Scalar& scalar) { return ScalarHashImpl(scalar).hash_; }

StringScalar::StringScalar(std::string s)
    : StringScalar(Buffer::FromString(std::move(s))) {}

FixedSizeBinaryScalar::FixedSizeBinaryScalar(std::shared_ptr<Buffer> value,
                                             std::shared_ptr<DataType> type)
    : BinaryScalar(std::move(value), std::move(type)) {
  ARROW_CHECK_EQ(checked_cast<const FixedSizeBinaryType&>(*this->type).byte_width(),
                 this->value->size());
}

BaseListScalar::BaseListScalar(std::shared_ptr<Array> value,
                               std::shared_ptr<DataType> type)
    : Scalar{std::move(type), true}, value(std::move(value)) {}

BaseListScalar::BaseListScalar(std::shared_ptr<Array> value)
    : Scalar(value->type(), true), value(std::move(value)) {}

FixedSizeListScalar::FixedSizeListScalar(std::shared_ptr<Array> value,
                                         std::shared_ptr<DataType> type)
    : BaseListScalar(std::move(value), std::move(type)) {
  ARROW_CHECK_EQ(this->value->length(),
                 checked_cast<const FixedSizeListType&>(*this->type).list_size());
}

DictionaryScalar::DictionaryScalar(std::shared_ptr<DataType> type)
    : Scalar(std::move(type)),
      value(
          MakeNullScalar(checked_cast<const DictionaryType&>(*this->type).value_type())) {
}

template <typename T>
using scalar_constructor_has_arrow_type =
    std::is_constructible<typename TypeTraits<T>::ScalarType, std::shared_ptr<DataType>>;

template <typename T, typename R = void>
using enable_if_scalar_constructor_has_arrow_type =
    typename std::enable_if<scalar_constructor_has_arrow_type<T>::value, R>::type;

template <typename T, typename R = void>
using enable_if_scalar_constructor_has_no_arrow_type =
    typename std::enable_if<!scalar_constructor_has_arrow_type<T>::value, R>::type;

struct MakeNullImpl {
  template <typename T, typename ScalarType = typename TypeTraits<T>::ScalarType>
  enable_if_scalar_constructor_has_arrow_type<T, Status> Visit(const T&) {
    out_ = std::make_shared<ScalarType>(type_);
    return Status::OK();
  }

  template <typename T, typename ScalarType = typename TypeTraits<T>::ScalarType>
  enable_if_scalar_constructor_has_no_arrow_type<T, Status> Visit(const T&) {
    out_ = std::make_shared<ScalarType>();
    return Status::OK();
  }

  std::shared_ptr<Scalar> Finish() && {
    // Should not fail.
    DCHECK_OK(VisitTypeInline(*type_, this));
    return std::move(out_);
  }

  std::shared_ptr<DataType> type_;
  std::shared_ptr<Scalar> out_;
};

std::shared_ptr<Scalar> MakeNullScalar(std::shared_ptr<DataType> type) {
  return MakeNullImpl{std::move(type), nullptr}.Finish();
}

std::string Scalar::ToString() const {
  auto maybe_repr = CastTo(utf8());
  if (maybe_repr.ok()) {
    return checked_cast<const StringScalar&>(*maybe_repr.ValueOrDie()).value->ToString();
  }
  return "...";
}

struct ScalarParseImpl {
  template <typename T, typename Converter = internal::StringConverter<T>,
            typename Value = typename Converter::value_type>
  Status Visit(const T& t) {
    Value value;
    if (!Converter{type_}(s_.data(), s_.size(), &value)) {
      return Status::Invalid("error parsing '", s_, "' as scalar of type ", t);
    }
    return Finish(std::move(value));
  }

  Status Visit(const BinaryType&) { return FinishWithBuffer(); }

  Status Visit(const LargeBinaryType&) { return FinishWithBuffer(); }

  Status Visit(const FixedSizeBinaryType&) { return FinishWithBuffer(); }

  Status Visit(const DictionaryType& t) {
    ARROW_ASSIGN_OR_RAISE(auto value, Scalar::Parse(t.value_type(), s_));
    return Finish(std::move(value));
  }

  Status Visit(const DataType& t) {
    return Status::NotImplemented("parsing scalars of type ", t);
  }

  template <typename Arg>
  Status Finish(Arg&& arg) {
    return MakeScalar(std::move(type_), std::forward<Arg>(arg)).Value(&out_);
  }

  Status FinishWithBuffer() { return Finish(Buffer::FromString(s_.to_string())); }

  Result<std::shared_ptr<Scalar>> Finish() && {
    RETURN_NOT_OK(VisitTypeInline(*type_, this));
    return std::move(out_);
  }

  ScalarParseImpl(std::shared_ptr<DataType> type, util::string_view s)
      : type_(std::move(type)), s_(s) {}

  std::shared_ptr<DataType> type_;
  util::string_view s_;
  std::shared_ptr<Scalar> out_;
};

Result<std::shared_ptr<Scalar>> Scalar::Parse(const std::shared_ptr<DataType>& type,
                                              util::string_view s) {
  return ScalarParseImpl{type, s}.Finish();
}

namespace internal {
Status CheckBufferLength(const FixedSizeBinaryType* t, const std::shared_ptr<Buffer>* b) {
  return t->byte_width() == (*b)->size()
             ? Status::OK()
             : Status::Invalid("buffer length ", (*b)->size(), " is not compatible with ",
                               *t);
}
}  // namespace internal

namespace {
// CastImpl(...) assumes `to` points to a non null scalar of the correct type with
// uninitialized value

// error fallback
Status CastImpl(const Scalar& from, Scalar* to) {
  return Status::NotImplemented("casting scalars of type ", *from.type, " to type ",
                                *to->type);
}

// numeric to numeric
template <typename From, typename To>
Status CastImpl(const NumericScalar<From>& from, NumericScalar<To>* to) {
  to->value = static_cast<typename To::c_type>(from.value);
  return Status::OK();
}

// numeric to boolean
template <typename T>
Status CastImpl(const NumericScalar<T>& from, BooleanScalar* to) {
  constexpr auto zero = static_cast<typename T::c_type>(0);
  to->value = from.value != zero;
  return Status::OK();
}

// boolean to numeric
template <typename T>
Status CastImpl(const BooleanScalar& from, NumericScalar<T>* to) {
  to->value = static_cast<typename T::c_type>(from.value);
  return Status::OK();
}

// numeric to temporal
template <typename From, typename To>
typename std::enable_if<std::is_base_of<TemporalType, To>::value &&
                            !std::is_same<DayTimeIntervalType, To>::value,
                        Status>::type
CastImpl(const NumericScalar<From>& from, TemporalScalar<To>* to) {
  to->value = static_cast<typename To::c_type>(from.value);
  return Status::OK();
}

// temporal to numeric
template <typename From, typename To>
typename std::enable_if<std::is_base_of<TemporalType, From>::value &&
                            !std::is_same<DayTimeIntervalType, From>::value,
                        Status>::type
CastImpl(const TemporalScalar<From>& from, NumericScalar<To>* to) {
  to->value = static_cast<typename To::c_type>(from.value);
  return Status::OK();
}

// timestamp to timestamp
Status CastImpl(const TimestampScalar& from, TimestampScalar* to) {
  return util::ConvertTimestampValue(from.type, to->type, from.value).Value(&to->value);
}

template <typename TypeWithTimeUnit>
std::shared_ptr<DataType> AsTimestampType(const std::shared_ptr<DataType>& type) {
  return timestamp(checked_cast<const TypeWithTimeUnit&>(*type).unit());
}

// duration to duration
Status CastImpl(const DurationScalar& from, DurationScalar* to) {
  return util::ConvertTimestampValue(AsTimestampType<DurationType>(from.type),
                                     AsTimestampType<DurationType>(to->type), from.value)
      .Value(&to->value);
}

// time to time
template <typename F, typename ToScalar, typename T = typename ToScalar::TypeClass>
enable_if_time<T, Status> CastImpl(const TimeScalar<F>& from, ToScalar* to) {
  return util::ConvertTimestampValue(AsTimestampType<F>(from.type),
                                     AsTimestampType<T>(to->type), from.value)
      .Value(&to->value);
}

constexpr int64_t kMillisecondsInDay = 86400000;

// date to date
Status CastImpl(const Date32Scalar& from, Date64Scalar* to) {
  to->value = from.value * kMillisecondsInDay;
  return Status::OK();
}
Status CastImpl(const Date64Scalar& from, Date32Scalar* to) {
  to->value = static_cast<int32_t>(from.value / kMillisecondsInDay);
  return Status::OK();
}

// timestamp to date
Status CastImpl(const TimestampScalar& from, Date64Scalar* to) {
  ARROW_ASSIGN_OR_RAISE(
      auto millis,
      util::ConvertTimestampValue(from.type, timestamp(TimeUnit::MILLI), from.value));
  to->value = millis - millis % kMillisecondsInDay;
  return Status::OK();
}
Status CastImpl(const TimestampScalar& from, Date32Scalar* to) {
  ARROW_ASSIGN_OR_RAISE(
      auto millis,
      util::ConvertTimestampValue(from.type, timestamp(TimeUnit::MILLI), from.value));
  to->value = static_cast<int32_t>(millis / kMillisecondsInDay);
  return Status::OK();
}

// date to timestamp
template <typename D>
Status CastImpl(const DateScalar<D>& from, TimestampScalar* to) {
  int64_t millis = from.value;
  if (std::is_same<D, Date32Type>::value) {
    millis *= kMillisecondsInDay;
  }
  return util::ConvertTimestampValue(timestamp(TimeUnit::MILLI), to->type, millis)
      .Value(&to->value);
}

Status CastImpl(const TimestampScalar& from, StringScalar* to) {
  to->value = Buffer::FromString(std::to_string(from.value));
  return Status::OK();
}

// string to any
template <typename ScalarType>
Status CastImpl(const StringScalar& from, ScalarType* to) {
  ARROW_ASSIGN_OR_RAISE(auto out,
                        Scalar::Parse(to->type, util::string_view(*from.value)));
  to->value = std::move(checked_cast<ScalarType&>(*out).value);
  return Status::OK();
}

// binary to string
Status CastImpl(const BinaryScalar& from, StringScalar* to) {
  to->value = from.value;
  return Status::OK();
}

// formattable to string
template <typename ScalarType, typename T = typename ScalarType::TypeClass,
          typename Formatter = internal::StringFormatter<T>,
          // note: Value unused but necessary to trigger SFINAE if Formatter is
          // undefined
          typename Value = typename Formatter::value_type>
Status CastImpl(const ScalarType& from, StringScalar* to) {
  if (!from.is_valid) {
    to->value = Buffer::FromString("null");
    return Status::OK();
  }

  return Formatter{from.type}(from.value, [to](util::string_view v) {
    to->value = Buffer::FromString(v.to_string());
    return Status::OK();
  });
}

template <typename ToType>
struct FromTypeVisitor {
  using ToScalar = typename TypeTraits<ToType>::ScalarType;

  template <typename FromType>
  Status Visit(const FromType&) {
    return CastImpl(checked_cast<const typename TypeTraits<FromType>::ScalarType&>(from_),
                    out_);
  }

  // identity cast only for parameter free types
  template <typename T1 = ToType>
  typename std::enable_if<TypeTraits<T1>::is_parameter_free, Status>::type Visit(
      const ToType&) {
    out_->value = checked_cast<const ToScalar&>(from_).value;
    return Status::OK();
  }

  // null to any
  Status Visit(const NullType&) {
    return Status::Invalid("attempting to cast scalar of type null to ", *to_type_);
  }

  Status Visit(const UnionType&) { return Status::NotImplemented("cast to ", *to_type_); }
  Status Visit(const DictionaryType&) {
    return Status::NotImplemented("cast to ", *to_type_);
  }
  Status Visit(const ExtensionType&) {
    return Status::NotImplemented("cast to ", *to_type_);
  }

  const Scalar& from_;
  const std::shared_ptr<DataType>& to_type_;
  ToScalar* out_;
};

struct ToTypeVisitor {
  template <typename ToType>
  Status Visit(const ToType&) {
    using ToScalar = typename TypeTraits<ToType>::ScalarType;
    FromTypeVisitor<ToType> unpack_from_type{from_, to_type_,
                                             checked_cast<ToScalar*>(out_)};
    return VisitTypeInline(*from_.type, &unpack_from_type);
  }

  Status Visit(const NullType&) {
    if (from_.is_valid) {
      return Status::Invalid("attempting to cast non-null scalar to NullScalar");
    }
    return Status::OK();
  }

  Status Visit(const UnionType&) {
    return Status::NotImplemented("cast from ", *from_.type);
  }
  Status Visit(const DictionaryType&) {
    return Status::NotImplemented("cast from ", *from_.type);
  }
  Status Visit(const ExtensionType&) {
    return Status::NotImplemented("cast from ", *from_.type);
  }

  const Scalar& from_;
  const std::shared_ptr<DataType>& to_type_;
  Scalar* out_;
};

}  // namespace

Result<std::shared_ptr<Scalar>> Scalar::CastTo(std::shared_ptr<DataType> to) const {
  std::shared_ptr<Scalar> out = MakeNullScalar(to);
  if (is_valid) {
    out->is_valid = true;
    ToTypeVisitor unpack_to_type{*this, to, out.get()};
    RETURN_NOT_OK(VisitTypeInline(*to, &unpack_to_type));
  }
  return out;
}

}  // namespace arrow
