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

#include "arrow/builder.h"

#include <string>
#include <utility>
#include <vector>

#include "arrow/status.h"
#include "arrow/type.h"
#include "arrow/util/checked_cast.h"
#include "arrow/util/hashing.h"
#include "arrow/visitor_inline.h"

namespace arrow {

class MemoryPool;

// ----------------------------------------------------------------------
// Helper functions

struct DictionaryBuilderCase {
  template <typename ValueType, typename Enable = typename ValueType::c_type>
  Status Visit(const ValueType&) {
    return CreateFor<ValueType>();
  }

  Status Visit(const BinaryType&) { return Create<BinaryDictionaryBuilder>(); }
  Status Visit(const StringType&) { return Create<StringDictionaryBuilder>(); }
  Status Visit(const FixedSizeBinaryType&) { return CreateFor<FixedSizeBinaryType>(); }

  Status Visit(const DataType& value_type) { return NotImplemented(value_type); }
  Status Visit(const HalfFloatType& value_type) { return NotImplemented(value_type); }
  Status NotImplemented(const DataType& value_type) {
    return Status::NotImplemented(
        "MakeBuilder: cannot construct builder for dictionaries with value type ",
        value_type);
  }

  template <typename ValueType>
  Status CreateFor() {
    return Create<DictionaryBuilder<ValueType>>();
  }

  template <typename BuilderType>
  Status Create() {
    if (dictionary != nullptr) {
      out->reset(new BuilderType(dictionary, pool));
    } else {
      out->reset(new BuilderType(value_type, pool));
    }
    return Status::OK();
  }

  Status Make() { return VisitTypeInline(*value_type, this); }

  MemoryPool* pool;
  const std::shared_ptr<DataType>& value_type;
  const std::shared_ptr<Array>& dictionary;
  std::unique_ptr<ArrayBuilder>* out;
};

#define BUILDER_CASE(ENUM, BuilderType)      \
  case Type::ENUM:                           \
    out->reset(new BuilderType(type, pool)); \
    return Status::OK();

Status MakeBuilder(MemoryPool* pool, const std::shared_ptr<DataType>& type,
                   std::unique_ptr<ArrayBuilder>* out) {
  switch (type->id()) {
    case Type::NA: {
      out->reset(new NullBuilder(pool));
      return Status::OK();
    }
      BUILDER_CASE(UINT8, UInt8Builder);
      BUILDER_CASE(INT8, Int8Builder);
      BUILDER_CASE(UINT16, UInt16Builder);
      BUILDER_CASE(INT16, Int16Builder);
      BUILDER_CASE(UINT32, UInt32Builder);
      BUILDER_CASE(INT32, Int32Builder);
      BUILDER_CASE(UINT64, UInt64Builder);
      BUILDER_CASE(INT64, Int64Builder);
      BUILDER_CASE(DATE32, Date32Builder);
      BUILDER_CASE(DATE64, Date64Builder);
      BUILDER_CASE(DURATION, DurationBuilder);
      BUILDER_CASE(TIME32, Time32Builder);
      BUILDER_CASE(TIME64, Time64Builder);
      BUILDER_CASE(TIMESTAMP, TimestampBuilder);
      BUILDER_CASE(BOOL, BooleanBuilder);
      BUILDER_CASE(HALF_FLOAT, HalfFloatBuilder);
      BUILDER_CASE(FLOAT, FloatBuilder);
      BUILDER_CASE(DOUBLE, DoubleBuilder);
      BUILDER_CASE(STRING, StringBuilder);
      BUILDER_CASE(BINARY, BinaryBuilder);
      BUILDER_CASE(LARGE_STRING, LargeStringBuilder);
      BUILDER_CASE(LARGE_BINARY, LargeBinaryBuilder);
      BUILDER_CASE(FIXED_SIZE_BINARY, FixedSizeBinaryBuilder);
      BUILDER_CASE(DECIMAL, Decimal128Builder);

    case Type::DICTIONARY: {
      const auto& dict_type = static_cast<const DictionaryType&>(*type);
      DictionaryBuilderCase visitor = {pool, dict_type.value_type(), nullptr, out};
      return visitor.Make();
    }

    case Type::INTERVAL: {
      const auto& interval_type = internal::checked_cast<const IntervalType&>(*type);
      if (interval_type.interval_type() == IntervalType::MONTHS) {
        out->reset(new MonthIntervalBuilder(type, pool));
        return Status::OK();
      }
      if (interval_type.interval_type() == IntervalType::DAY_TIME) {
        out->reset(new DayTimeIntervalBuilder(pool));
        return Status::OK();
      }
      break;
    }

    case Type::LIST: {
      std::unique_ptr<ArrayBuilder> value_builder;
      std::shared_ptr<DataType> value_type =
          internal::checked_cast<const ListType&>(*type).value_type();
      RETURN_NOT_OK(MakeBuilder(pool, value_type, &value_builder));
      out->reset(new ListBuilder(pool, std::move(value_builder), type));
      return Status::OK();
    }

    case Type::LARGE_LIST: {
      std::unique_ptr<ArrayBuilder> value_builder;
      std::shared_ptr<DataType> value_type =
          internal::checked_cast<const LargeListType&>(*type).value_type();
      RETURN_NOT_OK(MakeBuilder(pool, value_type, &value_builder));
      out->reset(new LargeListBuilder(pool, std::move(value_builder), type));
      return Status::OK();
    }

    case Type::MAP: {
      const auto& map_type = internal::checked_cast<const MapType&>(*type);
      std::unique_ptr<ArrayBuilder> key_builder, item_builder;
      RETURN_NOT_OK(MakeBuilder(pool, map_type.key_type(), &key_builder));
      RETURN_NOT_OK(MakeBuilder(pool, map_type.item_type(), &item_builder));
      out->reset(
          new MapBuilder(pool, std::move(key_builder), std::move(item_builder), type));
      return Status::OK();
    }

    case Type::FIXED_SIZE_LIST: {
      const auto& list_type = internal::checked_cast<const FixedSizeListType&>(*type);
      std::unique_ptr<ArrayBuilder> value_builder;
      auto value_type = list_type.value_type();
      RETURN_NOT_OK(MakeBuilder(pool, value_type, &value_builder));
      out->reset(new FixedSizeListBuilder(pool, std::move(value_builder), type));
      return Status::OK();
    }

    case Type::STRUCT: {
      const std::vector<std::shared_ptr<Field>>& fields = type->children();
      std::vector<std::shared_ptr<ArrayBuilder>> field_builders;

      for (const auto& it : fields) {
        std::unique_ptr<ArrayBuilder> builder;
        RETURN_NOT_OK(MakeBuilder(pool, it->type(), &builder));
        field_builders.emplace_back(std::move(builder));
      }
      out->reset(new StructBuilder(type, pool, std::move(field_builders)));
      return Status::OK();
    }

    case Type::UNION: {
      const auto& union_type = internal::checked_cast<const UnionType&>(*type);
      const std::vector<std::shared_ptr<Field>>& fields = type->children();
      std::vector<std::shared_ptr<ArrayBuilder>> field_builders;

      for (const auto& it : fields) {
        std::unique_ptr<ArrayBuilder> builder;
        RETURN_NOT_OK(MakeBuilder(pool, it->type(), &builder));
        field_builders.emplace_back(std::move(builder));
      }
      if (union_type.mode() == UnionMode::DENSE) {
        out->reset(new DenseUnionBuilder(pool, std::move(field_builders), type));
      } else {
        out->reset(new SparseUnionBuilder(pool, std::move(field_builders), type));
      }
      return Status::OK();
    }

    default:
      break;
  }
  return Status::NotImplemented("MakeBuilder: cannot construct builder for type ",
                                type->ToString());
}

Status MakeDictionaryBuilder(MemoryPool* pool, const std::shared_ptr<DataType>& type,
                             const std::shared_ptr<Array>& dictionary,
                             std::unique_ptr<ArrayBuilder>* out) {
  const auto& dict_type = static_cast<const DictionaryType&>(*type);
  DictionaryBuilderCase visitor = {pool, dict_type.value_type(), dictionary, out};
  return visitor.Make();
}

}  // namespace arrow
