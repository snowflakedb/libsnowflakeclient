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

#include "arrow/util/parsing.h"
#include "arrow/util/double_conversion.h"

namespace arrow {
namespace internal {

struct StringToFloatConverter::Impl {
  Impl()
      : main_converter_(flags_, main_junk_value_, main_junk_value_, "inf", "nan"),
        fallback_converter_(flags_, fallback_junk_value_, fallback_junk_value_, "inf",
                            "nan") {}

  // NOTE: This is only supported in double-conversion 3.1+
  static constexpr int flags_ =
      util::double_conversion::StringToDoubleConverter::ALLOW_CASE_INSENSIBILITY;

  // Two unlikely values to signal a parsing error
  static constexpr double main_junk_value_ = 0.7066424364107089;
  static constexpr double fallback_junk_value_ = 0.40088499148279166;

  util::double_conversion::StringToDoubleConverter main_converter_;
  util::double_conversion::StringToDoubleConverter fallback_converter_;
};

constexpr int StringToFloatConverter::Impl::flags_;
constexpr double StringToFloatConverter::Impl::main_junk_value_;
constexpr double StringToFloatConverter::Impl::fallback_junk_value_;

StringToFloatConverter::StringToFloatConverter() : impl_(new Impl()) {}

StringToFloatConverter::~StringToFloatConverter() {}

bool StringToFloatConverter::StringToFloat(const char* s, size_t length, float* out) {
  int processed_length;
  float v;
  v = impl_->main_converter_.StringToFloat(s, static_cast<int>(length),
                                           &processed_length);
  if (ARROW_PREDICT_FALSE(v == static_cast<float>(impl_->main_junk_value_))) {
    v = impl_->fallback_converter_.StringToFloat(s, static_cast<int>(length),
                                                 &processed_length);
    if (ARROW_PREDICT_FALSE(v == static_cast<float>(impl_->fallback_junk_value_))) {
      return false;
    }
  }
  *out = v;
  return true;
}

bool StringToFloatConverter::StringToFloat(const char* s, size_t length, double* out) {
  int processed_length;
  double v;
  v = impl_->main_converter_.StringToDouble(s, static_cast<int>(length),
                                            &processed_length);
  if (ARROW_PREDICT_FALSE(v == impl_->main_junk_value_)) {
    v = impl_->fallback_converter_.StringToDouble(s, static_cast<int>(length),
                                                  &processed_length);
    if (ARROW_PREDICT_FALSE(v == impl_->fallback_junk_value_)) {
      return false;
    }
  }
  *out = v;
  return true;
}

}  // namespace internal
}  // namespace arrow
