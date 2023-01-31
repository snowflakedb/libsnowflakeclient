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
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>

#include "arrow/status.h"
#include "arrow/util/bit_util.h"
#include "arrow/util/decimal.h"
#include "arrow/util/int_util.h"
#include "arrow/util/logging.h"
#include "arrow/util/macros.h"
#include "arrow/util/parsing.h"

namespace arrow {

using internal::SafeLeftShift;
using internal::SafeSignedAdd;

Decimal128::Decimal128(const std::string& str) : Decimal128() {
  *this = Decimal128::FromString(str).ValueOrDie();
}

static const Decimal128 kTenTo36(static_cast<int64_t>(0xC097CE7BC90715),
                                 0xB34B9F1000000000);
static const Decimal128 kTenTo18(0xDE0B6B3A7640000);

std::string Decimal128::ToIntegerString() const {
  Decimal128 remainder;
  std::stringstream buf;
  bool need_fill = false;

  // get anything above 10 ** 36 and print it
  Decimal128 top;
  std::tie(top, remainder) = Divide(kTenTo36).ValueOrDie();

  if (top != 0) {
    buf << static_cast<int64_t>(top);
    remainder.Abs();
    need_fill = true;
  }

  // now get anything above 10 ** 18 and print it
  Decimal128 tail;
  std::tie(top, tail) = remainder.Divide(kTenTo18).ValueOrDie();

  if (need_fill || top != 0) {
    if (need_fill) {
      buf << std::setw(18) << std::setfill('0');
    } else {
      need_fill = true;
      tail.Abs();
    }

    buf << static_cast<int64_t>(top);
  }

  // finally print the tail, which is less than 10**18
  if (need_fill) {
    buf << std::setw(18) << std::setfill('0');
  }
  buf << static_cast<int64_t>(tail);
  return buf.str();
}

Decimal128::operator int64_t() const {
  DCHECK(high_bits() == 0 || high_bits() == -1)
      << "Trying to cast a Decimal128 greater than the value range of a "
         "int64_t. high_bits_ must be equal to 0 or -1, got: "
      << high_bits();
  return static_cast<int64_t>(low_bits());
}

static std::string ToStringNegativeScale(const std::string& str,
                                         int32_t adjusted_exponent, bool is_negative) {
  std::stringstream buf;

  size_t offset = 0;
  buf << str[offset++];

  if (is_negative) {
    buf << str[offset++];
  }

  buf << '.' << str.substr(offset, std::string::npos) << 'E' << std::showpos
      << adjusted_exponent;
  return buf.str();
}

std::string Decimal128::ToString(int32_t scale) const {
  const std::string str(ToIntegerString());

  if (scale == 0) {
    return str;
  }

  const bool is_negative = *this < 0;

  const auto len = static_cast<int32_t>(str.size());
  const auto is_negative_offset = static_cast<int32_t>(is_negative);
  const int32_t adjusted_exponent = -scale + (len - 1 - is_negative_offset);

  /// Note that the -6 is taken from the Java BigDecimal documentation.
  if (scale < 0 || adjusted_exponent < -6) {
    return ToStringNegativeScale(str, adjusted_exponent, is_negative);
  }

  if (is_negative) {
    if (len - 1 > scale) {
      const auto n = static_cast<size_t>(len - scale);
      return str.substr(0, n) + "." + str.substr(n, static_cast<size_t>(scale));
    }

    if (len - 1 == scale) {
      return "-0." + str.substr(1, std::string::npos);
    }

    std::string result("-0." + std::string(static_cast<size_t>(scale - len + 1), '0'));
    return result + str.substr(1, std::string::npos);
  }

  if (len > scale) {
    const auto n = static_cast<size_t>(len - scale);
    return str.substr(0, n) + "." + str.substr(n, static_cast<size_t>(scale));
  }

  if (len == scale) {
    return "0." + str;
  }

  return "0." + std::string(static_cast<size_t>(scale - len), '0') + str;
}

static constexpr auto kInt64DecimalDigits =
    static_cast<size_t>(std::numeric_limits<int64_t>::digits10);
static constexpr int64_t kPowersOfTen[kInt64DecimalDigits + 1] = {1LL,
                                                                  10LL,
                                                                  100LL,
                                                                  1000LL,
                                                                  10000LL,
                                                                  100000LL,
                                                                  1000000LL,
                                                                  10000000LL,
                                                                  100000000LL,
                                                                  1000000000LL,
                                                                  10000000000LL,
                                                                  100000000000LL,
                                                                  1000000000000LL,
                                                                  10000000000000LL,
                                                                  100000000000000LL,
                                                                  1000000000000000LL,
                                                                  10000000000000000LL,
                                                                  100000000000000000LL,
                                                                  1000000000000000000LL};

// Iterates over data and for each group of kInt64DecimalDigits multiple out by
// the appropriate power of 10 necessary to add source parsed as uint64 and
// then adds the parsed value of source.
static inline void ShiftAndAdd(const char* data, size_t length, Decimal128* out) {
  internal::StringConverter<Int64Type> converter;
  for (size_t posn = 0; posn < length;) {
    const size_t group_size = std::min(kInt64DecimalDigits, length - posn);
    const int64_t multiple = kPowersOfTen[group_size];
    int64_t chunk = 0;
    ARROW_CHECK(converter(data + posn, group_size, &chunk));

    *out *= multiple;
    *out += chunk;
    posn += group_size;
  }
}

static void StringToInteger(const util::string_view whole_digits,
                            util::string_view fractional_digits, Decimal128* out) {
  using std::size_t;

  DCHECK_NE(out, nullptr) << "Decimal128 output variable cannot be nullptr";
  DCHECK_EQ(*out, 0)
      << "When converting a string to Decimal128 the initial output must be 0";

  DCHECK_GT(whole_digits.size() + fractional_digits.size(), 0)
      << "length of parsed decimal string should be greater than 0";

  ShiftAndAdd(whole_digits.data(), whole_digits.length(), out);
  ShiftAndAdd(fractional_digits.data(), fractional_digits.length(), out);
}

namespace {

struct DecimalComponents {
  util::string_view whole_digits;
  util::string_view fractional_digits;
  int32_t exponent = 0;
  char sign = 0;
  bool has_exponent = false;
};

inline bool IsSign(char c) { return c == '-' || c == '+'; }

inline bool IsDot(char c) { return c == '.'; }

inline bool IsDigit(char c) { return c >= '0' && c <= '9'; }

inline bool StartsExponent(char c) { return c == 'e' || c == 'E'; }

inline size_t ParseDigitsRun(const char* s, size_t start, size_t size,
                             util::string_view* out) {
  size_t pos;
  for (pos = start; pos < size; ++pos) {
    if (!IsDigit(s[pos])) {
      break;
    }
  }
  *out = util::string_view(s + start, pos - start);
  return pos;
}

bool ParseDecimalComponents(const char* s, size_t size, DecimalComponents* out) {
  size_t pos = 0;

  if (size == 0) {
    return false;
  }
  // Sign of the number
  if (IsSign(s[pos])) {
    out->sign = *(s + pos);
    ++pos;
  }
  // First run of digits
  pos = ParseDigitsRun(s, pos, size, &out->whole_digits);
  if (pos == size) {
    return !out->whole_digits.empty();
  }
  // Optional dot (if given in fractional form)
  bool has_dot = IsDot(s[pos]);
  if (has_dot) {
    // Second run of digits
    ++pos;
    pos = ParseDigitsRun(s, pos, size, &out->fractional_digits);
  }
  if (out->whole_digits.empty() && out->fractional_digits.empty()) {
    // Need at least some digits (whole or fractional)
    return false;
  }
  if (pos == size) {
    return true;
  }
  // Optional exponent
  if (StartsExponent(s[pos])) {
    ++pos;
    if (pos != size && s[pos] == '+') {
      ++pos;
    }
    out->has_exponent = true;
    internal::StringConverter<Int32Type> exponent_converter;
    return exponent_converter(s + pos, size - pos, &(out->exponent));
  }
  return pos == size;
}

}  // namespace

Status Decimal128::FromString(const util::string_view& s, Decimal128* out,
                              int32_t* precision, int32_t* scale) {
  if (s.empty()) {
    return Status::Invalid("Empty string cannot be converted to decimal");
  }

  DecimalComponents dec;
  if (!ParseDecimalComponents(s.data(), s.size(), &dec)) {
    return Status::Invalid("The string '", s, "' is not a valid decimal number");
  }

  // Count number of significant digits (without leading zeros)
  size_t first_non_zero = dec.whole_digits.find_first_not_of('0');
  size_t significant_digits = dec.fractional_digits.size();
  if (first_non_zero != std::string::npos) {
    significant_digits += dec.whole_digits.size() - first_non_zero;
  }

  if (precision != nullptr) {
    *precision = static_cast<int32_t>(significant_digits);
  }

  if (scale != nullptr) {
    if (dec.has_exponent) {
      auto adjusted_exponent = dec.exponent;
      auto len = static_cast<int32_t>(significant_digits);
      *scale = -adjusted_exponent + len - 1;
    } else {
      *scale = static_cast<int32_t>(dec.fractional_digits.size());
    }
  }

  if (out != nullptr) {
    *out = 0;
    StringToInteger(dec.whole_digits, dec.fractional_digits, out);

    if (dec.sign == '-') {
      out->Negate();
    }

    if (scale != nullptr && *scale < 0) {
      const int32_t abs_scale = std::abs(*scale);
      *out *= GetScaleMultiplier(abs_scale);

      if (precision != nullptr) {
        *precision += abs_scale;
      }
      *scale = 0;
    }
  }

  return Status::OK();
}

Status Decimal128::FromString(const std::string& s, Decimal128* out, int32_t* precision,
                              int32_t* scale) {
  return FromString(util::string_view(s), out, precision, scale);
}

Status Decimal128::FromString(const char* s, Decimal128* out, int32_t* precision,
                              int32_t* scale) {
  return FromString(util::string_view(s), out, precision, scale);
}

Result<Decimal128> Decimal128::FromString(const util::string_view& s) {
  Decimal128 out;
  RETURN_NOT_OK(FromString(s, &out, nullptr, nullptr));
  return std::move(out);
}

Result<Decimal128> Decimal128::FromString(const std::string& s) {
  return FromString(util::string_view(s));
}

Result<Decimal128> Decimal128::FromString(const char* s) {
  return FromString(util::string_view(s));
}

// Helper function used by Decimal128::FromBigEndian
static inline uint64_t UInt64FromBigEndian(const uint8_t* bytes, int32_t length) {
  // We don't bounds check the length here because this is called by
  // FromBigEndian that has a Decimal128 as its out parameters and
  // that function is already checking the length of the bytes and only
  // passes lengths between zero and eight.
  uint64_t result = 0;
  // Using memcpy instead of special casing for length
  // and doing the conversion in 16, 32 parts, which could
  // possibly create unaligned memory access on certain platforms
  memcpy(reinterpret_cast<uint8_t*>(&result) + 8 - length, bytes, length);
  return ::arrow::BitUtil::FromBigEndian(result);
}

Result<Decimal128> Decimal128::FromBigEndian(const uint8_t* bytes, int32_t length) {
  static constexpr int32_t kMinDecimalBytes = 1;
  static constexpr int32_t kMaxDecimalBytes = 16;

  int64_t high, low;

  if (length < kMinDecimalBytes || length > kMaxDecimalBytes) {
    return Status::Invalid("Length of byte array passed to Decimal128::FromBigEndian ",
                           "was ", length, ", but must be between ", kMinDecimalBytes,
                           " and ", kMaxDecimalBytes);
  }

  // Bytes are coming in big-endian, so the first byte is the MSB and therefore holds the
  // sign bit.
  const bool is_negative = static_cast<int8_t>(bytes[0]) < 0;

  // 1. Extract the high bytes
  // Stop byte of the high bytes
  const int32_t high_bits_offset = std::max(0, length - 8);
  const auto high_bits = UInt64FromBigEndian(bytes, high_bits_offset);

  if (high_bits_offset == 8) {
    // Avoid undefined shift by 64 below
    high = high_bits;
  } else {
    high = -1 * (is_negative && length < kMaxDecimalBytes);
    // Shift left enough bits to make room for the incoming int64_t
    high = SafeLeftShift(high, high_bits_offset * CHAR_BIT);
    // Preserve the upper bits by inplace OR-ing the int64_t
    high |= high_bits;
  }

  // 2. Extract the low bytes
  // Stop byte of the low bytes
  const int32_t low_bits_offset = std::min(length, 8);
  const auto low_bits =
      UInt64FromBigEndian(bytes + high_bits_offset, length - high_bits_offset);

  if (low_bits_offset == 8) {
    // Avoid undefined shift by 64 below
    low = low_bits;
  } else {
    // Sign extend the low bits if necessary
    low = -1 * (is_negative && length < 8);
    // Shift left enough bits to make room for the incoming int64_t
    low = SafeLeftShift(low, low_bits_offset * CHAR_BIT);
    // Preserve the upper bits by inplace OR-ing the int64_t
    low |= low_bits;
  }

  return Decimal128(high, static_cast<uint64_t>(low));
}

Status Decimal128::ToArrowStatus(DecimalStatus dstatus) const {
  Status status;

  switch (dstatus) {
    case DecimalStatus::kSuccess:
      status = Status::OK();
      break;

    case DecimalStatus::kDivideByZero:
      status = Status::Invalid("Division by 0 in Decimal128");
      break;

    case DecimalStatus::kOverflow:
      status = Status::Invalid("Overflow occurred during Decimal128 operation.");
      break;

    case DecimalStatus::kRescaleDataLoss:
      status = Status::Invalid("Rescaling decimal value would cause data loss");
      break;
  }
  return status;
}

std::ostream& operator<<(std::ostream& os, const Decimal128& decimal) {
  os << decimal.ToIntegerString();
  return os;
}

}  // namespace arrow
