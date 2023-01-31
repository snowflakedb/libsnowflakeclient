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
#include <utility>

#include "arrow/dataset/dataset_internal.h"
#include "arrow/dataset/filter.h"
#include "arrow/dataset/scanner.h"

namespace arrow {
namespace dataset {

inline RecordBatchIterator FilterRecordBatch(RecordBatchIterator it,
                                             const ExpressionEvaluator& evaluator,
                                             const Expression& filter, MemoryPool* pool) {
  return MakeMaybeMapIterator(
      [&filter, &evaluator, pool](std::shared_ptr<RecordBatch> in) {
        return evaluator.Evaluate(filter, *in, pool).Map([&](compute::Datum selection) {
          return evaluator.Filter(selection, in);
        });
      },
      std::move(it));
}

inline RecordBatchIterator ProjectRecordBatch(RecordBatchIterator it,
                                              RecordBatchProjector* projector,
                                              MemoryPool* pool) {
  return MakeMaybeMapIterator(
      [=](std::shared_ptr<RecordBatch> in) {
        // The RecordBatchProjector is shared accross ScanTasks of the same
        // Fragment. The resize operation of missing columns is not thread safe.
        // Ensure that each ScanTask gets his own projector.
        RecordBatchProjector local_projector{*projector};
        return local_projector.Project(*in, pool);
      },
      std::move(it));
}

class FilterAndProjectScanTask : public ScanTask {
 public:
  explicit FilterAndProjectScanTask(std::shared_ptr<ScanTask> task)
      : ScanTask(task->options(), task->context()), task_(std::move(task)) {}

  Result<RecordBatchIterator> Execute() override {
    ARROW_ASSIGN_OR_RAISE(auto it, task_->Execute());
    auto filter_it = FilterRecordBatch(std::move(it), *options_->evaluator,
                                       *options_->filter, context_->pool);
    return ProjectRecordBatch(std::move(filter_it), &task_->options()->projector,
                              context_->pool);
  }

 private:
  std::shared_ptr<ScanTask> task_;
};

/// \brief GetScanTaskIterator transforms an Iterator<Fragment> in a
/// flattened Iterator<ScanTask>.
inline ScanTaskIterator GetScanTaskIterator(FragmentIterator fragments,
                                            std::shared_ptr<ScanContext> context) {
  // Fragment -> ScanTaskIterator
  auto fn = [context](std::shared_ptr<Fragment> fragment) {
    return fragment->Scan(context);
  };

  // Iterator<Iterator<ScanTask>>
  auto maybe_scantask_it = MakeMaybeMapIterator(fn, std::move(fragments));

  // Iterator<ScanTask>
  return MakeFlattenIterator(std::move(maybe_scantask_it));
}

}  // namespace dataset
}  // namespace arrow
