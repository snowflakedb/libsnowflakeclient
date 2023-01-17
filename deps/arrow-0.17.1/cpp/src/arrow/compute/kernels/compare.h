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

#include <memory>

#include "arrow/compute/kernel.h"
#include "arrow/util/visibility.h"

namespace arrow {

class DataType;
class Status;

namespace compute {

class FunctionContext;

enum CompareOperator {
  EQUAL,
  NOT_EQUAL,
  GREATER,
  GREATER_EQUAL,
  LESS,
  LESS_EQUAL,
};

struct CompareOptions {
  explicit CompareOptions(CompareOperator op) : op(op) {}

  enum CompareOperator op;
};

/// \brief BinaryKernel bound implementing comparison
ARROW_EXPORT
Status MakeCompareKernel(const DataType& type, CompareOptions options,
                         std::shared_ptr<BinaryKernel>* out);

/// \brief Compare a numeric array with a scalar.
///
/// \param[in] context the FunctionContext
/// \param[in] left datum to compare, must be an Array
/// \param[in] right datum to compare, must be a Scalar of the same type than
///            left Datum.
/// \param[in] options compare options
/// \param[out] out resulting datum
///
/// Note on floating point arrays, this uses ieee-754 compare semantics.
///
/// \since 0.14.0
/// \note API not yet finalized
ARROW_EXPORT
Status Compare(FunctionContext* context, const Datum& left, const Datum& right,
               struct CompareOptions options, Datum* out);

}  // namespace compute
}  // namespace arrow
