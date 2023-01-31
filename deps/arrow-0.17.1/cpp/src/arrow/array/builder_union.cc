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

#include "arrow/array/builder_union.h"

#include <limits>
#include <utility>

#include "arrow/util/checked_cast.h"
#include "arrow/util/logging.h"

namespace arrow {

using internal::checked_cast;
using internal::checked_pointer_cast;

Status BasicUnionBuilder::FinishInternal(std::shared_ptr<ArrayData>* out) {
  std::shared_ptr<Buffer> types, null_bitmap;
  RETURN_NOT_OK(null_bitmap_builder_.Finish(&null_bitmap));
  RETURN_NOT_OK(types_builder_.Finish(&types));

  std::vector<std::shared_ptr<ArrayData>> child_data(children_.size());
  for (size_t i = 0; i < children_.size(); ++i) {
    RETURN_NOT_OK(children_[i]->FinishInternal(&child_data[i]));
  }

  *out = ArrayData::Make(type(), length(), {null_bitmap, types, nullptr}, null_count_);
  (*out)->child_data = std::move(child_data);
  return Status::OK();
}

BasicUnionBuilder::BasicUnionBuilder(
    MemoryPool* pool, UnionMode::type mode,
    const std::vector<std::shared_ptr<ArrayBuilder>>& children,
    const std::shared_ptr<DataType>& type)
    : ArrayBuilder(pool),
      child_fields_(children.size()),
      mode_(mode),
      types_builder_(pool) {
  DCHECK_EQ(type->id(), Type::UNION);
  const auto& union_type = checked_cast<const UnionType&>(*type);
  DCHECK_EQ(union_type.mode(), mode);
  DCHECK_EQ(children.size(), union_type.type_codes().size());

  type_codes_ = union_type.type_codes();
  children_ = children;

  type_id_to_children_.resize(union_type.max_type_code() + 1, nullptr);
  DCHECK_LT(
      type_id_to_children_.size(),
      static_cast<decltype(type_id_to_children_)::size_type>(UnionType::kMaxTypeCode));

  for (size_t i = 0; i < children.size(); ++i) {
    child_fields_[i] = union_type.child(static_cast<int>(i));

    auto type_id = union_type.type_codes()[i];
    type_id_to_children_[type_id] = children[i].get();
  }
}

BasicUnionBuilder::BasicUnionBuilder(MemoryPool* pool, UnionMode::type mode)
    : BasicUnionBuilder(pool, mode, {}, union_(mode)) {}

int8_t BasicUnionBuilder::AppendChild(const std::shared_ptr<ArrayBuilder>& new_child,
                                      const std::string& field_name) {
  children_.push_back(new_child);

  auto new_type_id = NextTypeId();

  type_id_to_children_[new_type_id] = new_child.get();

  child_fields_.push_back(field(field_name, nullptr));

  type_codes_.push_back(static_cast<int8_t>(new_type_id));

  return new_type_id;
}

std::shared_ptr<DataType> BasicUnionBuilder::type() const {
  std::vector<std::shared_ptr<Field>> child_fields(child_fields_.size());
  for (size_t i = 0; i < child_fields.size(); ++i) {
    child_fields[i] = child_fields_[i]->WithType(children_[i]->type());
  }
  return union_(std::move(child_fields), type_codes_, mode_);
}

int8_t BasicUnionBuilder::NextTypeId() {
  // Find type_id such that type_id_to_children_[type_id] == nullptr
  // and use that for the new child. Start searching at dense_type_id_
  // since type_id_to_children_ is densely packed up at least up to dense_type_id_
  for (; static_cast<size_t>(dense_type_id_) < type_id_to_children_.size();
       ++dense_type_id_) {
    if (type_id_to_children_[dense_type_id_] == nullptr) {
      return dense_type_id_++;
    }
  }

  DCHECK_LT(
      type_id_to_children_.size(),
      static_cast<decltype(type_id_to_children_)::size_type>(UnionType::kMaxTypeCode));

  // type_id_to_children_ is already densely packed, so just append the new child
  type_id_to_children_.resize(type_id_to_children_.size() + 1);
  return dense_type_id_++;
}

}  // namespace arrow
