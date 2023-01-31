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

#include "arrow/type.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <ostream>
#include <sstream>  // IWYU pragma: keep
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "arrow/array.h"
#include "arrow/compare.h"
#include "arrow/record_batch.h"
#include "arrow/result.h"
#include "arrow/status.h"
#include "arrow/table.h"
#include "arrow/util/checked_cast.h"
#include "arrow/util/hashing.h"
#include "arrow/util/key_value_metadata.h"
#include "arrow/util/logging.h"
#include "arrow/util/make_unique.h"
#include "arrow/util/vector.h"
#include "arrow/visitor_inline.h"

namespace arrow {
namespace {
using internal::checked_cast;

// Merges `existing` and `other` if one of them is of NullType, otherwise
// returns nullptr.
//   - if `other` if of NullType or is nullable, the unified field will be nullable.
//   - if `existing` is of NullType but other is not, the unified field will
//     have `other`'s type and will be nullable
std::shared_ptr<Field> MaybePromoteNullTypes(const Field& existing, const Field& other) {
  if (existing.type()->id() != Type::NA && other.type()->id() != Type::NA) {
    return nullptr;
  }
  if (existing.type()->id() == Type::NA) {
    return other.WithNullable(true)->WithMetadata(existing.metadata());
  }
  // `other` must be null.
  return existing.WithNullable(true);
}
}  // namespace

Field::~Field() {}

bool Field::HasMetadata() const {
  return (metadata_ != nullptr) && (metadata_->size() > 0);
}

std::shared_ptr<Field> Field::WithMetadata(
    const std::shared_ptr<const KeyValueMetadata>& metadata) const {
  return std::make_shared<Field>(name_, type_, nullable_, metadata);
}

std::shared_ptr<Field> Field::WithMergedMetadata(
    const std::shared_ptr<const KeyValueMetadata>& metadata) const {
  std::shared_ptr<const KeyValueMetadata> merged_metadata;
  if (metadata_) {
    merged_metadata = metadata_->Merge(*metadata);
  } else {
    merged_metadata = metadata;
  }
  return std::make_shared<Field>(name_, type_, nullable_, merged_metadata);
}

std::shared_ptr<Field> Field::RemoveMetadata() const {
  return std::make_shared<Field>(name_, type_, nullable_);
}

std::shared_ptr<Field> Field::WithType(const std::shared_ptr<DataType>& type) const {
  return std::make_shared<Field>(name_, type, nullable_, metadata_);
}

std::shared_ptr<Field> Field::WithName(const std::string& name) const {
  return std::make_shared<Field>(name, type_, nullable_, metadata_);
}

std::shared_ptr<Field> Field::WithNullable(const bool nullable) const {
  return std::make_shared<Field>(name_, type_, nullable, metadata_);
}

Result<std::shared_ptr<Field>> Field::MergeWith(const Field& other,
                                                MergeOptions options) const {
  if (name() != other.name()) {
    return Status::Invalid("Field ", name(), " doesn't have the same name as ",
                           other.name());
  }

  if (Equals(other, /*check_metadata=*/false)) {
    return Copy();
  }

  if (options.promote_nullability) {
    if (type()->Equals(other.type())) {
      return Copy()->WithNullable(nullable() || other.nullable());
    }
    std::shared_ptr<Field> promoted = MaybePromoteNullTypes(*this, other);
    if (promoted) return promoted;
  }

  return Status::Invalid("Unable to merge: Field ", name(),
                         " has incompatible types: ", type()->ToString(), " vs ",
                         other.type()->ToString());
}

Result<std::shared_ptr<Field>> Field::MergeWith(const std::shared_ptr<Field>& other,
                                                MergeOptions options) const {
  DCHECK_NE(other, nullptr);
  return MergeWith(*other, options);
}

std::vector<std::shared_ptr<Field>> Field::Flatten() const {
  std::vector<std::shared_ptr<Field>> flattened;
  if (type_->id() == Type::STRUCT) {
    for (const auto& child : type_->children()) {
      auto flattened_child = child->Copy();
      flattened.push_back(flattened_child);
      flattened_child->name_.insert(0, name() + ".");
      flattened_child->nullable_ |= nullable_;
    }
  } else {
    flattened.push_back(this->Copy());
  }
  return flattened;
}

std::shared_ptr<Field> Field::Copy() const {
  return ::arrow::field(name_, type_, nullable_, metadata_);
}

bool Field::Equals(const Field& other, bool check_metadata) const {
  if (this == &other) {
    return true;
  }
  if (this->name_ == other.name_ && this->nullable_ == other.nullable_ &&
      this->type_->Equals(*other.type_.get(), check_metadata)) {
    if (!check_metadata) {
      return true;
    } else if (this->HasMetadata() && other.HasMetadata()) {
      return metadata_->Equals(*other.metadata_);
    } else if (!this->HasMetadata() && !other.HasMetadata()) {
      return true;
    } else {
      return false;
    }
  }
  return false;
}

bool Field::Equals(const std::shared_ptr<Field>& other, bool check_metadata) const {
  return Equals(*other.get(), check_metadata);
}

bool Field::IsCompatibleWith(const Field& other) const { return MergeWith(other).ok(); }

bool Field::IsCompatibleWith(const std::shared_ptr<Field>& other) const {
  DCHECK_NE(other, nullptr);
  return IsCompatibleWith(*other);
}

std::string Field::ToString(bool show_metadata) const {
  std::stringstream ss;
  ss << name_ << ": " << type_->ToString();
  if (!nullable_) {
    ss << " not null";
  }
  if (show_metadata && metadata_) {
    ss << metadata_->ToString();
  }
  return ss.str();
}

DataType::~DataType() {}

bool DataType::Equals(const DataType& other, bool check_metadata) const {
  return TypeEquals(*this, other, check_metadata);
}

bool DataType::Equals(const std::shared_ptr<DataType>& other) const {
  if (!other) {
    return false;
  }
  return Equals(*other.get());
}

std::ostream& operator<<(std::ostream& os, const DataType& type) {
  os << type.ToString();
  return os;
}

FloatingPointType::Precision HalfFloatType::precision() const {
  return FloatingPointType::HALF;
}

FloatingPointType::Precision FloatType::precision() const {
  return FloatingPointType::SINGLE;
}

FloatingPointType::Precision DoubleType::precision() const {
  return FloatingPointType::DOUBLE;
}

std::string ListType::ToString() const {
  std::stringstream s;
  s << "list<" << value_field()->ToString() << ">";
  return s.str();
}

std::string LargeListType::ToString() const {
  std::stringstream s;
  s << "large_list<" << value_field()->ToString() << ">";
  return s.str();
}

MapType::MapType(const std::shared_ptr<DataType>& key_type,
                 const std::shared_ptr<DataType>& item_type, bool keys_sorted)
    : MapType(key_type, field("value", item_type), keys_sorted) {}

MapType::MapType(const std::shared_ptr<DataType>& key_type,
                 const std::shared_ptr<Field>& item_field, bool keys_sorted)
    : ListType(field(
          "entries",
          struct_({std::make_shared<Field>("key", key_type, false), item_field}), false)),
      keys_sorted_(keys_sorted) {
  id_ = type_id;
}

std::string MapType::ToString() const {
  std::stringstream s;
  s << "map<" << key_type()->ToString() << ", " << item_type()->ToString();
  if (keys_sorted_) {
    s << ", keys_sorted";
  }
  s << ">";
  return s.str();
}

std::string FixedSizeListType::ToString() const {
  std::stringstream s;
  s << "fixed_size_list<" << value_field()->ToString() << ">[" << list_size_ << "]";
  return s.str();
}

std::string BinaryType::ToString() const { return "binary"; }

std::string LargeBinaryType::ToString() const { return "large_binary"; }

std::string StringType::ToString() const { return "string"; }

std::string LargeStringType::ToString() const { return "large_string"; }

int FixedSizeBinaryType::bit_width() const { return CHAR_BIT * byte_width(); }

std::string FixedSizeBinaryType::ToString() const {
  std::stringstream ss;
  ss << "fixed_size_binary[" << byte_width_ << "]";
  return ss.str();
}

// ----------------------------------------------------------------------
// Date types

DateType::DateType(Type::type type_id) : TemporalType(type_id) {}

Date32Type::Date32Type() : DateType(Type::DATE32) {}

Date64Type::Date64Type() : DateType(Type::DATE64) {}

std::string Date64Type::ToString() const { return std::string("date64[ms]"); }

std::string Date32Type::ToString() const { return std::string("date32[day]"); }

// ----------------------------------------------------------------------
// Time types

TimeType::TimeType(Type::type type_id, TimeUnit::type unit)
    : TemporalType(type_id), unit_(unit) {}

Time32Type::Time32Type(TimeUnit::type unit) : TimeType(Type::TIME32, unit) {
  ARROW_CHECK(unit == TimeUnit::SECOND || unit == TimeUnit::MILLI)
      << "Must be seconds or milliseconds";
}

std::string Time32Type::ToString() const {
  std::stringstream ss;
  ss << "time32[" << this->unit_ << "]";
  return ss.str();
}

Time64Type::Time64Type(TimeUnit::type unit) : TimeType(Type::TIME64, unit) {
  ARROW_CHECK(unit == TimeUnit::MICRO || unit == TimeUnit::NANO)
      << "Must be microseconds or nanoseconds";
}

std::string Time64Type::ToString() const {
  std::stringstream ss;
  ss << "time64[" << this->unit_ << "]";
  return ss.str();
}

std::ostream& operator<<(std::ostream& os, TimeUnit::type unit) {
  switch (unit) {
    case TimeUnit::SECOND:
      os << "s";
      break;
    case TimeUnit::MILLI:
      os << "ms";
      break;
    case TimeUnit::MICRO:
      os << "us";
      break;
    case TimeUnit::NANO:
      os << "ns";
      break;
  }
  return os;
}

// ----------------------------------------------------------------------
// Timestamp types

std::string TimestampType::ToString() const {
  std::stringstream ss;
  ss << "timestamp[" << this->unit_;
  if (this->timezone_.size() > 0) {
    ss << ", tz=" << this->timezone_;
  }
  ss << "]";
  return ss.str();
}

// Duration types
std::string DurationType::ToString() const {
  std::stringstream ss;
  ss << "duration[" << this->unit_ << "]";
  return ss.str();
}

// ----------------------------------------------------------------------
// Union type

constexpr int8_t UnionType::kMaxTypeCode;
constexpr int UnionType::kInvalidChildId;

UnionType::UnionType(const std::vector<std::shared_ptr<Field>>& fields,
                     const std::vector<int8_t>& type_codes, UnionMode::type mode)
    : NestedType(Type::UNION),
      mode_(mode),
      type_codes_(type_codes),
      child_ids_(kMaxTypeCode + 1, kInvalidChildId) {
  DCHECK_OK(ValidateParameters(fields, type_codes, mode));
  children_ = fields;
  for (int child_id = 0; child_id < static_cast<int>(type_codes_.size()); ++child_id) {
    const auto type_code = type_codes_[child_id];
    child_ids_[type_code] = child_id;
  }
}

Result<std::shared_ptr<DataType>> UnionType::Make(
    const std::vector<std::shared_ptr<Field>>& fields,
    const std::vector<int8_t>& type_codes, UnionMode::type mode) {
  RETURN_NOT_OK(ValidateParameters(fields, type_codes, mode));
  return std::make_shared<UnionType>(fields, type_codes, mode);
}

Status UnionType::ValidateParameters(const std::vector<std::shared_ptr<Field>>& fields,
                                     const std::vector<int8_t>& type_codes,
                                     UnionMode::type mode) {
  if (fields.size() != type_codes.size()) {
    return Status::Invalid("Union should get the same number of fields as type codes");
  }
  for (const auto type_code : type_codes) {
    if (type_code < 0 || type_code > kMaxTypeCode) {
      return Status::Invalid("Union type code out of bounds");
    }
  }
  return Status::OK();
}

DataTypeLayout UnionType::layout() const {
  if (mode_ == UnionMode::SPARSE) {
    return DataTypeLayout({DataTypeLayout::Bitmap(),
                           DataTypeLayout::FixedWidth(sizeof(uint8_t)),
                           DataTypeLayout::AlwaysNull()});
  } else {
    return DataTypeLayout({DataTypeLayout::Bitmap(),
                           DataTypeLayout::FixedWidth(sizeof(uint8_t)),
                           DataTypeLayout::FixedWidth(sizeof(int32_t))});
  }
}

uint8_t UnionType::max_type_code() const {
  return type_codes_.size() == 0
             ? 0
             : *std::max_element(type_codes_.begin(), type_codes_.end());
}

std::string UnionType::ToString() const {
  std::stringstream s;

  if (mode_ == UnionMode::SPARSE) {
    s << "union[sparse]<";
  } else {
    s << "union[dense]<";
  }

  for (size_t i = 0; i < children_.size(); ++i) {
    if (i) {
      s << ", ";
    }
    s << children_[i]->ToString() << "=" << static_cast<int>(type_codes_[i]);
  }
  s << ">";
  return s.str();
}

// ----------------------------------------------------------------------
// Struct type

namespace {

std::unordered_multimap<std::string, int> CreateNameToIndexMap(
    const std::vector<std::shared_ptr<Field>>& fields) {
  std::unordered_multimap<std::string, int> name_to_index;
  for (size_t i = 0; i < fields.size(); ++i) {
    name_to_index.emplace(fields[i]->name(), static_cast<int>(i));
  }
  return name_to_index;
}

template <int NotFoundValue = -1, int DuplicateFoundValue = -1>
int LookupNameIndex(const std::unordered_multimap<std::string, int>& name_to_index,
                    const std::string& name) {
  auto p = name_to_index.equal_range(name);
  auto it = p.first;
  if (it == p.second) {
    // Not found
    return NotFoundValue;
  }
  auto index = it->second;
  if (++it != p.second) {
    // Duplicate field name
    return DuplicateFoundValue;
  }
  return index;
}

}  // namespace

class StructType::Impl {
 public:
  explicit Impl(const std::vector<std::shared_ptr<Field>>& fields)
      : name_to_index_(CreateNameToIndexMap(fields)) {}

  const std::unordered_multimap<std::string, int> name_to_index_;
};

StructType::StructType(const std::vector<std::shared_ptr<Field>>& fields)
    : NestedType(Type::STRUCT), impl_(new Impl(fields)) {
  children_ = fields;
}

StructType::~StructType() {}

std::string StructType::ToString() const {
  std::stringstream s;
  s << "struct<";
  for (int i = 0; i < this->num_children(); ++i) {
    if (i > 0) {
      s << ", ";
    }
    std::shared_ptr<Field> field = this->child(i);
    s << field->ToString();
  }
  s << ">";
  return s.str();
}

std::shared_ptr<Field> StructType::GetFieldByName(const std::string& name) const {
  int i = GetFieldIndex(name);
  return i == -1 ? nullptr : children_[i];
}

int StructType::GetFieldIndex(const std::string& name) const {
  return LookupNameIndex(impl_->name_to_index_, name);
}

std::vector<int> StructType::GetAllFieldIndices(const std::string& name) const {
  std::vector<int> result;
  auto p = impl_->name_to_index_.equal_range(name);
  for (auto it = p.first; it != p.second; ++it) {
    result.push_back(it->second);
  }
  if (result.size() > 1) {
    std::sort(result.begin(), result.end());
  }
  return result;
}

std::vector<std::shared_ptr<Field>> StructType::GetAllFieldsByName(
    const std::string& name) const {
  std::vector<std::shared_ptr<Field>> result;
  auto p = impl_->name_to_index_.equal_range(name);
  for (auto it = p.first; it != p.second; ++it) {
    result.push_back(children_[it->second]);
  }
  return result;
}

// ----------------------------------------------------------------------
// Decimal128 type

Decimal128Type::Decimal128Type(int32_t precision, int32_t scale)
    : DecimalType(16, precision, scale) {
  ARROW_CHECK_GE(precision, kMinPrecision);
  ARROW_CHECK_LE(precision, kMaxPrecision);
}

Result<std::shared_ptr<DataType>> Decimal128Type::Make(int32_t precision, int32_t scale) {
  if (precision < kMinPrecision || precision > kMaxPrecision) {
    return Status::Invalid("Decimal precision out of range: ", precision);
  }
  return std::make_shared<Decimal128Type>(precision, scale);
}

Status Decimal128Type::Make(int32_t precision, int32_t scale,
                            std::shared_ptr<DataType>* out) {
  return Make(precision, scale).Value(out);
}

// ----------------------------------------------------------------------
// Dictionary-encoded type

Status DictionaryType::ValidateParameters(const DataType& index_type,
                                          const DataType& value_type) {
  const bool index_type_ok = is_integer(index_type.id()) &&
                             checked_cast<const IntegerType&>(index_type).is_signed();
  if (!index_type_ok) {
    return Status::TypeError("Dictionary index type should be signed integer, got ",
                             index_type.ToString());
  }
  return Status::OK();
}

int DictionaryType::bit_width() const {
  return checked_cast<const FixedWidthType&>(*index_type_).bit_width();
}

Result<std::shared_ptr<DataType>> DictionaryType::Make(
    const std::shared_ptr<DataType>& index_type,
    const std::shared_ptr<DataType>& value_type, bool ordered) {
  RETURN_NOT_OK(ValidateParameters(*index_type, *value_type));
  return std::make_shared<DictionaryType>(index_type, value_type, ordered);
}

DictionaryType::DictionaryType(const std::shared_ptr<DataType>& index_type,
                               const std::shared_ptr<DataType>& value_type, bool ordered)
    : FixedWidthType(Type::DICTIONARY),
      index_type_(index_type),
      value_type_(value_type),
      ordered_(ordered) {
  ARROW_CHECK_OK(ValidateParameters(*index_type_, *value_type_));
}

DataTypeLayout DictionaryType::layout() const {
  auto layout = index_type_->layout();
  layout.has_dictionary = true;
  return layout;
}

std::string DictionaryType::ToString() const {
  std::stringstream ss;
  ss << this->name() << "<values=" << value_type_->ToString()
     << ", indices=" << index_type_->ToString() << ", ordered=" << ordered_ << ">";
  return ss.str();
}

// ----------------------------------------------------------------------
// Null type

std::string NullType::ToString() const { return name(); }

// ----------------------------------------------------------------------
// FieldRef

size_t FieldPath::hash() const {
  return internal::ComputeStringHash<0>(indices().data(), indices().size() * sizeof(int));
}

std::string FieldPath::ToString() const {
  std::string repr = "FieldPath(";
  for (auto index : this->indices()) {
    repr += std::to_string(index) + " ";
  }
  repr.resize(repr.size() - 1);
  repr += ")";
  return repr;
}

struct FieldPathGetImpl {
  static const DataType& GetType(const ArrayData& data) { return *data.type; }

  static const DataType& GetType(const ChunkedArray& array) { return *array.type(); }

  static void Summarize(const FieldVector& fields, std::stringstream* ss) {
    *ss << "{ ";
    for (const auto& field : fields) {
      *ss << field->ToString() << ", ";
    }
    *ss << "}";
  }

  template <typename T>
  static void Summarize(const std::vector<T>& columns, std::stringstream* ss) {
    *ss << "{ ";
    for (const auto& column : columns) {
      *ss << GetType(*column) << ", ";
    }
    *ss << "}";
  }

  template <typename T>
  static Status IndexError(const FieldPath* path, int out_of_range_depth,
                           const std::vector<T>& children) {
    std::stringstream ss;
    ss << "index out of range. ";

    ss << "indices=[ ";
    int depth = 0;
    for (int i : path->indices()) {
      if (depth != out_of_range_depth) {
        ss << i << " ";
        continue;
      }
      ss << ">" << i << "< ";
      ++depth;
    }
    ss << "] ";

    if (std::is_same<T, std::shared_ptr<Field>>::value) {
      ss << "fields were: ";
    } else {
      ss << "columns had types: ";
    }
    Summarize(children, &ss);

    return Status::IndexError(ss.str());
  }

  template <typename T, typename GetChildren>
  static Result<T> Get(const FieldPath* path, const std::vector<T>* children,
                       GetChildren&& get_children, int* out_of_range_depth) {
    if (path->indices().empty()) {
      return Status::Invalid("empty indices cannot be traversed");
    }

    int depth = 0;
    const T* out;
    for (int index : path->indices()) {
      if (index < 0 || static_cast<size_t>(index) >= children->size()) {
        *out_of_range_depth = depth;
        return nullptr;
      }

      out = &children->at(index);
      children = get_children(*out);
      ++depth;
    }

    return *out;
  }

  template <typename T, typename GetChildren>
  static Result<T> Get(const FieldPath* path, const std::vector<T>* children,
                       GetChildren&& get_children) {
    int out_of_range_depth = -1;
    ARROW_ASSIGN_OR_RAISE(auto child,
                          Get(path, children, std::forward<GetChildren>(get_children),
                              &out_of_range_depth));
    if (child != nullptr) {
      return std::move(child);
    }
    return IndexError(path, out_of_range_depth, *children);
  }

  static Result<std::shared_ptr<Field>> Get(const FieldPath* path,
                                            const FieldVector& fields) {
    return FieldPathGetImpl::Get(path, &fields, [](const std::shared_ptr<Field>& field) {
      return &field->type()->children();
    });
  }

  static Result<std::shared_ptr<ArrayData>> Get(const FieldPath* path,
                                                const ArrayDataVector& child_data) {
    return FieldPathGetImpl::Get(
        path, &child_data,
        [](const std::shared_ptr<ArrayData>& data) { return &data->child_data; });
  }

  static Result<std::shared_ptr<ChunkedArray>> Get(
      const FieldPath* path, const ChunkedArrayVector& columns_arg) {
    ChunkedArrayVector columns = columns_arg;

    return FieldPathGetImpl::Get(
        path, &columns, [&](const std::shared_ptr<ChunkedArray>& a) {
          columns.clear();

          for (int i = 0; i < a->type()->num_children(); ++i) {
            ArrayVector child_chunks;

            for (const auto& chunk : a->chunks()) {
              auto child_chunk = MakeArray(chunk->data()->child_data[i]);
              child_chunks.push_back(std::move(child_chunk));
            }

            auto child_column = std::make_shared<ChunkedArray>(
                std::move(child_chunks), a->type()->child(i)->type());

            columns.emplace_back(std::move(child_column));
          }

          return &columns;
        });
  }
};

Result<std::shared_ptr<Field>> FieldPath::Get(const Schema& schema) const {
  return FieldPathGetImpl::Get(this, schema.fields());
}

Result<std::shared_ptr<Field>> FieldPath::Get(const Field& field) const {
  return FieldPathGetImpl::Get(this, field.type()->children());
}

Result<std::shared_ptr<Field>> FieldPath::Get(const DataType& type) const {
  return FieldPathGetImpl::Get(this, type.children());
}

Result<std::shared_ptr<Field>> FieldPath::Get(const FieldVector& fields) const {
  return FieldPathGetImpl::Get(this, fields);
}

Result<std::shared_ptr<Array>> FieldPath::Get(const RecordBatch& batch) const {
  ARROW_ASSIGN_OR_RAISE(auto data, FieldPathGetImpl::Get(this, batch.column_data()));
  return MakeArray(data);
}

Result<std::shared_ptr<ChunkedArray>> FieldPath::Get(const Table& table) const {
  return FieldPathGetImpl::Get(this, table.columns());
}

FieldRef::FieldRef(FieldPath indices) : impl_(std::move(indices)) {
  DCHECK_GT(util::get<FieldPath>(impl_).indices().size(), 0);
}

void FieldRef::Flatten(std::vector<FieldRef> children) {
  // flatten children
  struct Visitor {
    void operator()(std::string&& name) { *out++ = FieldRef(std::move(name)); }

    void operator()(FieldPath&& indices) { *out++ = FieldRef(std::move(indices)); }

    void operator()(std::vector<FieldRef>&& children) {
      for (auto& child : children) {
        util::visit(*this, std::move(child.impl_));
      }
    }

    std::back_insert_iterator<std::vector<FieldRef>> out;
  };

  std::vector<FieldRef> out;
  Visitor visitor{std::back_inserter(out)};
  visitor(std::move(children));

  DCHECK(!out.empty());
  DCHECK(std::none_of(out.begin(), out.end(),
                      [](const FieldRef& ref) { return ref.IsNested(); }));

  if (out.size() == 1) {
    impl_ = std::move(out[0].impl_);
  } else {
    impl_ = std::move(out);
  }
}

Result<FieldRef> FieldRef::FromDotPath(const std::string& dot_path_arg) {
  if (dot_path_arg.empty()) {
    return Status::Invalid("Dot path was empty");
  }

  std::vector<FieldRef> children;

  util::string_view dot_path = dot_path_arg;

  auto parse_name = [&] {
    std::string name;
    for (;;) {
      auto segment_end = dot_path.find_first_of("\\[.");
      if (segment_end == util::string_view::npos) {
        // dot_path doesn't contain any other special characters; consume all
        name.append(dot_path.begin(), dot_path.end());
        dot_path = "";
        break;
      }

      if (dot_path[segment_end] != '\\') {
        // segment_end points to a subscript for a new FieldRef
        name.append(dot_path.begin(), segment_end);
        dot_path = dot_path.substr(segment_end);
        break;
      }

      if (dot_path.size() == segment_end + 1) {
        // dot_path ends with backslash; consume it all
        name.append(dot_path.begin(), dot_path.end());
        dot_path = "";
        break;
      }

      // append all characters before backslash, then the character which follows it
      name.append(dot_path.begin(), segment_end);
      name.push_back(dot_path[segment_end + 1]);
      dot_path = dot_path.substr(segment_end + 2);
    }
    return name;
  };

  while (!dot_path.empty()) {
    auto subscript = dot_path[0];
    dot_path = dot_path.substr(1);
    switch (subscript) {
      case '.': {
        // next element is a name
        children.emplace_back(parse_name());
        continue;
      }
      case '[': {
        auto subscript_end = dot_path.find_first_not_of("0123456789");
        if (subscript_end == util::string_view::npos || dot_path[subscript_end] != ']') {
          return Status::Invalid("Dot path '", dot_path_arg,
                                 "' contained an unterminated index");
        }
        children.emplace_back(std::atoi(dot_path.data()));
        dot_path = dot_path.substr(subscript_end + 1);
        continue;
      }
      default:
        return Status::Invalid("Dot path must begin with '[' or '.', got '", dot_path_arg,
                               "'");
    }
  }

  FieldRef out;
  out.Flatten(std::move(children));
  return out;
}

size_t FieldRef::hash() const {
  struct Visitor : std::hash<std::string> {
    using std::hash<std::string>::operator();

    size_t operator()(const FieldPath& path) { return path.hash(); }

    size_t operator()(const std::vector<FieldRef>& children) {
      size_t hash = 0;

      for (const FieldRef& child : children) {
        hash ^= child.hash();
      }

      return hash;
    }
  };

  return util::visit(Visitor{}, impl_);
}

std::string FieldRef::ToString() const {
  struct Visitor {
    std::string operator()(const FieldPath& path) { return path.ToString(); }

    std::string operator()(const std::string& name) { return "Name(" + name + ")"; }

    std::string operator()(const std::vector<FieldRef>& children) {
      std::string repr = "Nested(";
      for (const auto& child : children) {
        repr += child.ToString() + " ";
      }
      repr.resize(repr.size() - 1);
      repr += ")";
      return repr;
    }
  };

  return "FieldRef." + util::visit(Visitor{}, impl_);
}

std::vector<FieldPath> FieldRef::FindAll(const Schema& schema) const {
  return FindAll(schema.fields());
}

std::vector<FieldPath> FieldRef::FindAll(const Field& field) const {
  return FindAll(field.type()->children());
}

std::vector<FieldPath> FieldRef::FindAll(const DataType& type) const {
  return FindAll(type.children());
}

std::vector<FieldPath> FieldRef::FindAll(const FieldVector& fields) const {
  struct Visitor {
    std::vector<FieldPath> operator()(const FieldPath& path) {
      // skip long IndexError construction if path is out of range
      int out_of_range_depth;
      auto maybe_field = FieldPathGetImpl::Get(
          &path, &fields_,
          [](const std::shared_ptr<Field>& field) { return &field->type()->children(); },
          &out_of_range_depth);

      DCHECK_OK(maybe_field.status());

      if (maybe_field.ValueOrDie() != nullptr) {
        return {path};
      }
      return {};
    }

    std::vector<FieldPath> operator()(const std::string& name) {
      std::vector<FieldPath> out;

      for (int i = 0; i < static_cast<int>(fields_.size()); ++i) {
        if (fields_[i]->name() == name) {
          out.push_back({i});
        }
      }

      return out;
    }

    struct Matches {
      // referents[i] is referenced by prefixes[i]
      std::vector<FieldPath> prefixes;
      FieldVector referents;

      Matches(std::vector<FieldPath> matches, const FieldVector& fields) {
        for (auto& match : matches) {
          Add({}, std::move(match), fields);
        }
      }

      Matches() = default;

      size_t size() const { return referents.size(); }

      void Add(FieldPath prefix, const FieldPath& match, const FieldVector& fields) {
        auto maybe_field = match.Get(fields);
        DCHECK_OK(maybe_field.status());

        prefix.indices().resize(prefix.indices().size() + match.indices().size());
        std::copy(match.indices().begin(), match.indices().end(),
                  prefix.indices().end() - match.indices().size());
        prefixes.push_back(std::move(prefix));
        referents.push_back(std::move(maybe_field).ValueOrDie());
      }
    };

    std::vector<FieldPath> operator()(const std::vector<FieldRef>& refs) {
      DCHECK_GE(refs.size(), 1);
      Matches matches(refs.front().FindAll(fields_), fields_);

      for (auto ref_it = refs.begin() + 1; ref_it != refs.end(); ++ref_it) {
        Matches next_matches;
        for (size_t i = 0; i < matches.size(); ++i) {
          const auto& referent = *matches.referents[i];

          for (const FieldPath& match : ref_it->FindAll(referent)) {
            next_matches.Add(matches.prefixes[i], match, referent.type()->children());
          }
        }
        matches = std::move(next_matches);
      }

      return matches.prefixes;
    }

    const FieldVector& fields_;
  };

  return util::visit(Visitor{fields}, impl_);
}

void PrintTo(const FieldRef& ref, std::ostream* os) { *os << ref.ToString(); }

// ----------------------------------------------------------------------
// Schema implementation

class Schema::Impl {
 public:
  Impl(std::vector<std::shared_ptr<Field>> fields,
       std::shared_ptr<const KeyValueMetadata> metadata)
      : fields_(std::move(fields)),
        name_to_index_(CreateNameToIndexMap(fields_)),
        metadata_(std::move(metadata)) {}

  std::vector<std::shared_ptr<Field>> fields_;
  std::unordered_multimap<std::string, int> name_to_index_;
  std::shared_ptr<const KeyValueMetadata> metadata_;
};

Schema::Schema(std::vector<std::shared_ptr<Field>> fields,
               std::shared_ptr<const KeyValueMetadata> metadata)
    : detail::Fingerprintable(),
      impl_(new Impl(std::move(fields), std::move(metadata))) {}

Schema::Schema(const Schema& schema)
    : detail::Fingerprintable(), impl_(new Impl(*schema.impl_)) {}

Schema::~Schema() {}

int Schema::num_fields() const { return static_cast<int>(impl_->fields_.size()); }

const std::shared_ptr<Field>& Schema::field(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, num_fields());
  return impl_->fields_[i];
}

const std::vector<std::shared_ptr<Field>>& Schema::fields() const {
  return impl_->fields_;
}

bool Schema::Equals(const Schema& other, bool check_metadata) const {
  if (this == &other) {
    return true;
  }

  // checks field equality
  if (num_fields() != other.num_fields()) {
    return false;
  }

  if (check_metadata) {
    const auto& metadata_fp = metadata_fingerprint();
    const auto& other_metadata_fp = other.metadata_fingerprint();
    if (metadata_fp != other_metadata_fp) {
      return false;
    }
  }

  // Fast path using fingerprints, if possible
  const auto& fp = fingerprint();
  const auto& other_fp = other.fingerprint();
  if (!fp.empty() && !other_fp.empty()) {
    return fp == other_fp;
  }

  // Fall back on field-by-field comparison
  for (int i = 0; i < num_fields(); ++i) {
    if (!field(i)->Equals(*other.field(i).get(), check_metadata)) {
      return false;
    }
  }

  return true;
}

bool Schema::Equals(const std::shared_ptr<Schema>& other, bool check_metadata) const {
  if (other == nullptr) {
    return false;
  }

  return Equals(*other, check_metadata);
}

std::shared_ptr<Field> Schema::GetFieldByName(const std::string& name) const {
  int i = GetFieldIndex(name);
  return i == -1 ? nullptr : impl_->fields_[i];
}

int Schema::GetFieldIndex(const std::string& name) const {
  return LookupNameIndex(impl_->name_to_index_, name);
}

std::vector<int> Schema::GetAllFieldIndices(const std::string& name) const {
  std::vector<int> result;
  auto p = impl_->name_to_index_.equal_range(name);
  for (auto it = p.first; it != p.second; ++it) {
    result.push_back(it->second);
  }
  if (result.size() > 1) {
    std::sort(result.begin(), result.end());
  }
  return result;
}

Status Schema::CanReferenceFieldsByNames(const std::vector<std::string>& names) const {
  for (const auto& name : names) {
    if (GetFieldByName(name) == nullptr) {
      return Status::Invalid("Field named '", name,
                             "' not found or not unique in the schema.");
    }
  }

  return Status::OK();
}

std::vector<std::shared_ptr<Field>> Schema::GetAllFieldsByName(
    const std::string& name) const {
  std::vector<std::shared_ptr<Field>> result;
  auto p = impl_->name_to_index_.equal_range(name);
  for (auto it = p.first; it != p.second; ++it) {
    result.push_back(impl_->fields_[it->second]);
  }
  return result;
}

Result<std::shared_ptr<Schema>> Schema::AddField(
    int i, const std::shared_ptr<Field>& field) const {
  if (i < 0 || i > this->num_fields()) {
    return Status::Invalid("Invalid column index to add field.");
  }

  return std::make_shared<Schema>(internal::AddVectorElement(impl_->fields_, i, field),
                                  impl_->metadata_);
}

Result<std::shared_ptr<Schema>> Schema::SetField(
    int i, const std::shared_ptr<Field>& field) const {
  if (i < 0 || i > this->num_fields()) {
    return Status::Invalid("Invalid column index to add field.");
  }

  return std::make_shared<Schema>(
      internal::ReplaceVectorElement(impl_->fields_, i, field), impl_->metadata_);
}

Result<std::shared_ptr<Schema>> Schema::RemoveField(int i) const {
  if (i < 0 || i >= this->num_fields()) {
    return Status::Invalid("Invalid column index to remove field.");
  }

  return std::make_shared<Schema>(internal::DeleteVectorElement(impl_->fields_, i),
                                  impl_->metadata_);
}

Status Schema::AddField(int i, const std::shared_ptr<Field>& field,
                        std::shared_ptr<Schema>* out) const {
  return AddField(i, field).Value(out);
}

Status Schema::SetField(int i, const std::shared_ptr<Field>& field,
                        std::shared_ptr<Schema>* out) const {
  return SetField(i, field).Value(out);
}

Status Schema::RemoveField(int i, std::shared_ptr<Schema>* out) const {
  return RemoveField(i).Value(out);
}

bool Schema::HasMetadata() const {
  return (impl_->metadata_ != nullptr) && (impl_->metadata_->size() > 0);
}

bool Schema::HasDistinctFieldNames() const {
  auto fields = field_names();
  std::unordered_set<std::string> names{fields.cbegin(), fields.cend()};
  return names.size() == fields.size();
}

std::shared_ptr<Schema> Schema::WithMetadata(
    const std::shared_ptr<const KeyValueMetadata>& metadata) const {
  return std::make_shared<Schema>(impl_->fields_, metadata);
}

std::shared_ptr<const KeyValueMetadata> Schema::metadata() const {
  return impl_->metadata_;
}

std::shared_ptr<Schema> Schema::RemoveMetadata() const {
  return std::make_shared<Schema>(impl_->fields_);
}

std::string Schema::ToString(bool show_metadata) const {
  std::stringstream buffer;

  int i = 0;
  for (const auto& field : impl_->fields_) {
    if (i > 0) {
      buffer << std::endl;
    }
    buffer << field->ToString();
    ++i;
  }

  if (show_metadata && HasMetadata()) {
    buffer << impl_->metadata_->ToString();
  }

  return buffer.str();
}

std::vector<std::string> Schema::field_names() const {
  std::vector<std::string> names;
  for (const auto& field : impl_->fields_) {
    names.push_back(field->name());
  }
  return names;
}

class SchemaBuilder::Impl {
 public:
  friend class SchemaBuilder;
  Impl(ConflictPolicy policy, Field::MergeOptions field_merge_options)
      : policy_(policy), field_merge_options_(field_merge_options) {}

  Impl(std::vector<std::shared_ptr<Field>> fields,
       std::shared_ptr<const KeyValueMetadata> metadata, ConflictPolicy conflict_policy,
       Field::MergeOptions field_merge_options)
      : fields_(std::move(fields)),
        name_to_index_(CreateNameToIndexMap(fields_)),
        metadata_(std::move(metadata)),
        policy_(conflict_policy),
        field_merge_options_(field_merge_options) {}

  Status AddField(const std::shared_ptr<Field>& field) {
    DCHECK_NE(field, nullptr);

    // Short-circuit, no lookup needed.
    if (policy_ == CONFLICT_APPEND) {
      return AppendField(field);
    }

    auto name = field->name();
    constexpr int kNotFound = -1;
    constexpr int kDuplicateFound = -2;
    auto i = LookupNameIndex<kNotFound, kDuplicateFound>(name_to_index_, name);

    if (i == kNotFound) {
      return AppendField(field);
    }

    // From this point, there's one or more field in the builder that exists with
    // the same name.

    if (policy_ == CONFLICT_IGNORE) {
      // The ignore policy is more generous when there's duplicate in the builder.
      return Status::OK();
    } else if (policy_ == CONFLICT_ERROR) {
      return Status::Invalid("Duplicate found, policy dictate to treat as an error");
    }

    if (i == kDuplicateFound) {
      // Cannot merge/replace when there's more than one field in the builder
      // because we can't decide which to merge/replace.
      return Status::Invalid("Cannot merge field ", name,
                             " more than one field with same name exists");
    }

    DCHECK_GE(i, 0);

    if (policy_ == CONFLICT_REPLACE) {
      fields_[i] = field;
    } else if (policy_ == CONFLICT_MERGE) {
      ARROW_ASSIGN_OR_RAISE(fields_[i], fields_[i]->MergeWith(field));
    }

    return Status::OK();
  }

  Status AppendField(const std::shared_ptr<Field>& field) {
    name_to_index_.emplace(field->name(), static_cast<int>(fields_.size()));
    fields_.push_back(field);
    return Status::OK();
  }

  void Reset() {
    fields_.clear();
    name_to_index_.clear();
    metadata_.reset();
  }

 private:
  std::vector<std::shared_ptr<Field>> fields_;
  std::unordered_multimap<std::string, int> name_to_index_;
  std::shared_ptr<const KeyValueMetadata> metadata_;
  ConflictPolicy policy_;
  Field::MergeOptions field_merge_options_;
};

SchemaBuilder::SchemaBuilder(ConflictPolicy policy,
                             Field::MergeOptions field_merge_options) {
  impl_ = internal::make_unique<Impl>(policy, field_merge_options);
}

SchemaBuilder::SchemaBuilder(std::vector<std::shared_ptr<Field>> fields,
                             ConflictPolicy policy,
                             Field::MergeOptions field_merge_options) {
  impl_ = internal::make_unique<Impl>(std::move(fields), nullptr, policy,
                                      field_merge_options);
}

SchemaBuilder::SchemaBuilder(const std::shared_ptr<Schema>& schema, ConflictPolicy policy,
                             Field::MergeOptions field_merge_options) {
  std::shared_ptr<const KeyValueMetadata> metadata;
  if (schema->HasMetadata()) {
    metadata = schema->metadata()->Copy();
  }

  impl_ = internal::make_unique<Impl>(schema->fields(), std::move(metadata), policy,
                                      field_merge_options);
}

SchemaBuilder::~SchemaBuilder() {}

SchemaBuilder::ConflictPolicy SchemaBuilder::policy() const { return impl_->policy_; }

void SchemaBuilder::SetPolicy(SchemaBuilder::ConflictPolicy resolution) {
  impl_->policy_ = resolution;
}

Status SchemaBuilder::AddField(const std::shared_ptr<Field>& field) {
  return impl_->AddField(field);
}

Status SchemaBuilder::AddFields(const std::vector<std::shared_ptr<Field>>& fields) {
  for (const auto& field : fields) {
    RETURN_NOT_OK(AddField(field));
  }

  return Status::OK();
}

Status SchemaBuilder::AddSchema(const std::shared_ptr<Schema>& schema) {
  DCHECK_NE(schema, nullptr);
  return AddFields(schema->fields());
}

Status SchemaBuilder::AddSchemas(const std::vector<std::shared_ptr<Schema>>& schemas) {
  for (const auto& schema : schemas) {
    RETURN_NOT_OK(AddSchema(schema));
  }

  return Status::OK();
}

Status SchemaBuilder::AddMetadata(const KeyValueMetadata& metadata) {
  impl_->metadata_ = metadata.Copy();
  return Status::OK();
}

Result<std::shared_ptr<Schema>> SchemaBuilder::Finish() const {
  return schema(impl_->fields_, impl_->metadata_);
}

void SchemaBuilder::Reset() { impl_->Reset(); }

Result<std::shared_ptr<Schema>> SchemaBuilder::Merge(
    const std::vector<std::shared_ptr<Schema>>& schemas, ConflictPolicy policy) {
  SchemaBuilder builder{policy};
  RETURN_NOT_OK(builder.AddSchemas(schemas));
  return builder.Finish();
}

Status SchemaBuilder::AreCompatible(const std::vector<std::shared_ptr<Schema>>& schemas,
                                    ConflictPolicy policy) {
  return Merge(schemas, policy).status();
}

std::shared_ptr<Schema> schema(std::vector<std::shared_ptr<Field>> fields,
                               std::shared_ptr<const KeyValueMetadata> metadata) {
  return std::make_shared<Schema>(std::move(fields), std::move(metadata));
}

Result<std::shared_ptr<Schema>> UnifySchemas(
    const std::vector<std::shared_ptr<Schema>>& schemas,
    const Field::MergeOptions field_merge_options) {
  if (schemas.empty()) {
    return Status::Invalid("Must provide at least one schema to unify.");
  }

  if (!schemas[0]->HasDistinctFieldNames()) {
    return Status::Invalid("Can't unify schema with duplicate field names.");
  }

  SchemaBuilder builder{schemas[0], SchemaBuilder::CONFLICT_MERGE, field_merge_options};

  for (size_t i = 1; i < schemas.size(); i++) {
    const auto& schema = schemas[i];
    if (!schema->HasDistinctFieldNames()) {
      return Status::Invalid("Can't unify schema with duplicate field names.");
    }
    RETURN_NOT_OK(builder.AddSchema(schema));
  }

  return builder.Finish();
}

// ----------------------------------------------------------------------
// Fingerprint computations

namespace detail {

Fingerprintable::~Fingerprintable() {
  delete fingerprint_.load();
  delete metadata_fingerprint_.load();
}

template <typename ComputeFingerprint>
static const std::string& LoadFingerprint(std::atomic<std::string*>* fingerprint,
                                          ComputeFingerprint&& compute_fingerprint) {
  auto new_p = new std::string(std::forward<ComputeFingerprint>(compute_fingerprint)());
  // Since fingerprint() and metadata_fingerprint() return a *reference* to the
  // allocated string, the first allocation ever should never be replaced by another
  // one.  Hence the compare_exchange_strong() against nullptr.
  std::string* expected = nullptr;
  if (fingerprint->compare_exchange_strong(expected, new_p)) {
    return *new_p;
  } else {
    delete new_p;
    DCHECK_NE(expected, nullptr);
    return *expected;
  }
}

const std::string& Fingerprintable::LoadFingerprintSlow() const {
  return LoadFingerprint(&fingerprint_, [this]() { return ComputeFingerprint(); });
}

const std::string& Fingerprintable::LoadMetadataFingerprintSlow() const {
  return LoadFingerprint(&metadata_fingerprint_,
                         [this]() { return ComputeMetadataFingerprint(); });
}

}  // namespace detail

static inline std::string TypeIdFingerprint(const DataType& type) {
  auto c = static_cast<int>(type.id()) + 'A';
  DCHECK_GE(c, 0);
  DCHECK_LT(c, 128);  // Unlikely to happen any soon
  // Prefix with an unusual character in order to disambiguate
  std::string s{'@', static_cast<char>(c)};
  return s;
}

static char TimeUnitFingerprint(TimeUnit::type unit) {
  switch (unit) {
    case TimeUnit::SECOND:
      return 's';
    case TimeUnit::MILLI:
      return 'm';
    case TimeUnit::MICRO:
      return 'u';
    case TimeUnit::NANO:
      return 'n';
    default:
      DCHECK(false) << "Unexpected TimeUnit";
      return '\0';
  }
}

static char IntervalTypeFingerprint(IntervalType::type unit) {
  switch (unit) {
    case IntervalType::DAY_TIME:
      return 'd';
    case IntervalType::MONTHS:
      return 'M';
    default:
      DCHECK(false) << "Unexpected IntervalType::type";
      return '\0';
  }
}

static void AppendMetadataFingerprint(const KeyValueMetadata& metadata,
                                      std::stringstream* ss) {
  // Compute metadata fingerprint.  KeyValueMetadata is not immutable,
  // so we don't cache the result on the metadata instance.
  const auto pairs = metadata.sorted_pairs();
  if (!pairs.empty()) {
    *ss << "!{";
    for (const auto& p : pairs) {
      const auto& k = p.first;
      const auto& v = p.second;
      // Since metadata strings can contain arbitrary characters, prefix with
      // string length to disambiguate.
      *ss << k.length() << ':' << k << ':';
      *ss << v.length() << ':' << v << ';';
    }
    *ss << '}';
  }
}

std::string Field::ComputeFingerprint() const {
  const auto& type_fingerprint = type_->fingerprint();
  if (type_fingerprint.empty()) {
    // Underlying DataType doesn't support fingerprinting.
    return "";
  }
  std::stringstream ss;
  ss << 'F';
  if (nullable_) {
    ss << 'n';
  } else {
    ss << 'N';
  }
  ss << name_;
  ss << '{' << type_fingerprint << '}';
  return ss.str();
}

std::string Field::ComputeMetadataFingerprint() const {
  std::stringstream ss;
  if (metadata_) {
    AppendMetadataFingerprint(*metadata_, &ss);
  }
  const auto& type_fingerprint = type_->metadata_fingerprint();
  if (!type_fingerprint.empty()) {
    ss << "+{" << type_->metadata_fingerprint() << "}";
  }
  return ss.str();
}

std::string Schema::ComputeFingerprint() const {
  std::stringstream ss;
  ss << "S{";
  for (const auto& field : fields()) {
    const auto& field_fingerprint = field->fingerprint();
    if (field_fingerprint.empty()) {
      return "";
    }
    ss << field_fingerprint << ";";
  }
  ss << "}";
  return ss.str();
}

std::string Schema::ComputeMetadataFingerprint() const {
  std::stringstream ss;
  if (HasMetadata()) {
    AppendMetadataFingerprint(*metadata(), &ss);
  }
  ss << "S{";
  for (const auto& field : fields()) {
    const auto& field_fingerprint = field->metadata_fingerprint();
    ss << field_fingerprint << ";";
  }
  ss << "}";
  return ss.str();
}

void PrintTo(const Schema& s, std::ostream* os) { *os << s; }

std::string DataType::ComputeFingerprint() const {
  // Default implementation returns empty string, signalling non-implemented
  // functionality.
  return "";
}

std::string DataType::ComputeMetadataFingerprint() const {
  // Whatever the data type, metadata can only be found on child fields
  std::string s;
  for (const auto& child : children_) {
    s += child->metadata_fingerprint() + ";";
  }
  return s;
}

#define PARAMETER_LESS_FINGERPRINT(TYPE_CLASS)               \
  std::string TYPE_CLASS##Type::ComputeFingerprint() const { \
    return TypeIdFingerprint(*this);                         \
  }

PARAMETER_LESS_FINGERPRINT(Null)
PARAMETER_LESS_FINGERPRINT(Boolean)
PARAMETER_LESS_FINGERPRINT(Int8)
PARAMETER_LESS_FINGERPRINT(Int16)
PARAMETER_LESS_FINGERPRINT(Int32)
PARAMETER_LESS_FINGERPRINT(Int64)
PARAMETER_LESS_FINGERPRINT(UInt8)
PARAMETER_LESS_FINGERPRINT(UInt16)
PARAMETER_LESS_FINGERPRINT(UInt32)
PARAMETER_LESS_FINGERPRINT(UInt64)
PARAMETER_LESS_FINGERPRINT(HalfFloat)
PARAMETER_LESS_FINGERPRINT(Float)
PARAMETER_LESS_FINGERPRINT(Double)
PARAMETER_LESS_FINGERPRINT(Binary)
PARAMETER_LESS_FINGERPRINT(LargeBinary)
PARAMETER_LESS_FINGERPRINT(String)
PARAMETER_LESS_FINGERPRINT(LargeString)
PARAMETER_LESS_FINGERPRINT(Date32)
PARAMETER_LESS_FINGERPRINT(Date64)

#undef PARAMETER_LESS_FINGERPRINT

std::string DictionaryType::ComputeFingerprint() const {
  const auto& index_fingerprint = index_type_->fingerprint();
  const auto& value_fingerprint = value_type_->fingerprint();
  std::string ordered_fingerprint = ordered_ ? "1" : "0";

  DCHECK(!index_fingerprint.empty());  // it's an integer type
  if (!value_fingerprint.empty()) {
    return TypeIdFingerprint(*this) + index_fingerprint + value_fingerprint +
           ordered_fingerprint;
  }
  return ordered_fingerprint;
}

std::string ListType::ComputeFingerprint() const {
  const auto& child_fingerprint = children_[0]->fingerprint();
  if (!child_fingerprint.empty()) {
    return TypeIdFingerprint(*this) + "{" + child_fingerprint + "}";
  }
  return "";
}

std::string LargeListType::ComputeFingerprint() const {
  const auto& child_fingerprint = children_[0]->fingerprint();
  if (!child_fingerprint.empty()) {
    return TypeIdFingerprint(*this) + "{" + child_fingerprint + "}";
  }
  return "";
}

std::string MapType::ComputeFingerprint() const {
  const auto& child_fingerprint = children_[0]->fingerprint();
  if (!child_fingerprint.empty()) {
    if (keys_sorted_) {
      return TypeIdFingerprint(*this) + "s{" + child_fingerprint + "}";
    } else {
      return TypeIdFingerprint(*this) + "{" + child_fingerprint + "}";
    }
  }
  return "";
}

std::string FixedSizeListType::ComputeFingerprint() const {
  const auto& child_fingerprint = children_[0]->fingerprint();
  if (!child_fingerprint.empty()) {
    std::stringstream ss;
    ss << TypeIdFingerprint(*this) << "[" << list_size_ << "]"
       << "{" << child_fingerprint << "}";
    return ss.str();
  }
  return "";
}

std::string FixedSizeBinaryType::ComputeFingerprint() const {
  std::stringstream ss;
  ss << TypeIdFingerprint(*this) << "[" << byte_width_ << "]";
  return ss.str();
}

std::string DecimalType::ComputeFingerprint() const {
  std::stringstream ss;
  ss << TypeIdFingerprint(*this) << "[" << byte_width_ << "," << precision_ << ","
     << scale_ << "]";
  return ss.str();
}

std::string StructType::ComputeFingerprint() const {
  std::stringstream ss;
  ss << TypeIdFingerprint(*this) << "{";
  for (const auto& child : children_) {
    const auto& child_fingerprint = child->fingerprint();
    if (child_fingerprint.empty()) {
      return "";
    }
    ss << child_fingerprint << ";";
  }
  ss << "}";
  return ss.str();
}

std::string UnionType::ComputeFingerprint() const {
  std::stringstream ss;
  ss << TypeIdFingerprint(*this);
  switch (mode_) {
    case UnionMode::SPARSE:
      ss << "[s";
      break;
    case UnionMode::DENSE:
      ss << "[d";
      break;
    default:
      DCHECK(false) << "Unexpected UnionMode";
  }
  for (const auto code : type_codes_) {
    // Represent code as integer, not raw character
    ss << ':' << static_cast<int32_t>(code);
  }
  ss << "]{";
  for (const auto& child : children_) {
    const auto& child_fingerprint = child->fingerprint();
    if (child_fingerprint.empty()) {
      return "";
    }
    ss << child_fingerprint << ";";
  }
  ss << "}";
  return ss.str();
}

std::string TimeType::ComputeFingerprint() const {
  std::stringstream ss;
  ss << TypeIdFingerprint(*this) << TimeUnitFingerprint(unit_);
  return ss.str();
}

std::string TimestampType::ComputeFingerprint() const {
  std::stringstream ss;
  ss << TypeIdFingerprint(*this) << TimeUnitFingerprint(unit_) << timezone_.length()
     << ':' << timezone_;
  return ss.str();
}

std::string IntervalType::ComputeFingerprint() const {
  std::stringstream ss;
  ss << TypeIdFingerprint(*this) << IntervalTypeFingerprint(interval_type());
  return ss.str();
}

std::string DurationType::ComputeFingerprint() const {
  std::stringstream ss;
  ss << TypeIdFingerprint(*this) << TimeUnitFingerprint(unit_);
  return ss.str();
}

// ----------------------------------------------------------------------
// Visitors and factory functions

Status DataType::Accept(TypeVisitor* visitor) const {
  return VisitTypeInline(*this, visitor);
}

#define TYPE_FACTORY(NAME, KLASS)                                        \
  std::shared_ptr<DataType> NAME() {                                     \
    static std::shared_ptr<DataType> result = std::make_shared<KLASS>(); \
    return result;                                                       \
  }

TYPE_FACTORY(null, NullType)
TYPE_FACTORY(boolean, BooleanType)
TYPE_FACTORY(int8, Int8Type)
TYPE_FACTORY(uint8, UInt8Type)
TYPE_FACTORY(int16, Int16Type)
TYPE_FACTORY(uint16, UInt16Type)
TYPE_FACTORY(int32, Int32Type)
TYPE_FACTORY(uint32, UInt32Type)
TYPE_FACTORY(int64, Int64Type)
TYPE_FACTORY(uint64, UInt64Type)
TYPE_FACTORY(float16, HalfFloatType)
TYPE_FACTORY(float32, FloatType)
TYPE_FACTORY(float64, DoubleType)
TYPE_FACTORY(utf8, StringType)
TYPE_FACTORY(large_utf8, LargeStringType)
TYPE_FACTORY(binary, BinaryType)
TYPE_FACTORY(large_binary, LargeBinaryType)
TYPE_FACTORY(date64, Date64Type)
TYPE_FACTORY(date32, Date32Type)

std::shared_ptr<DataType> fixed_size_binary(int32_t byte_width) {
  return std::make_shared<FixedSizeBinaryType>(byte_width);
}

std::shared_ptr<DataType> duration(TimeUnit::type unit) {
  return std::make_shared<DurationType>(unit);
}

std::shared_ptr<DataType> day_time_interval() {
  return std::make_shared<DayTimeIntervalType>();
}

std::shared_ptr<DataType> month_interval() {
  return std::make_shared<MonthIntervalType>();
}

std::shared_ptr<DataType> timestamp(TimeUnit::type unit) {
  return std::make_shared<TimestampType>(unit);
}

std::shared_ptr<DataType> timestamp(TimeUnit::type unit, const std::string& timezone) {
  return std::make_shared<TimestampType>(unit, timezone);
}

std::shared_ptr<DataType> time32(TimeUnit::type unit) {
  return std::make_shared<Time32Type>(unit);
}

std::shared_ptr<DataType> time64(TimeUnit::type unit) {
  return std::make_shared<Time64Type>(unit);
}

std::shared_ptr<DataType> list(const std::shared_ptr<DataType>& value_type) {
  return std::make_shared<ListType>(value_type);
}

std::shared_ptr<DataType> list(const std::shared_ptr<Field>& value_field) {
  return std::make_shared<ListType>(value_field);
}

std::shared_ptr<DataType> large_list(const std::shared_ptr<DataType>& value_type) {
  return std::make_shared<LargeListType>(value_type);
}

std::shared_ptr<DataType> large_list(const std::shared_ptr<Field>& value_field) {
  return std::make_shared<LargeListType>(value_field);
}

std::shared_ptr<DataType> map(const std::shared_ptr<DataType>& key_type,
                              const std::shared_ptr<DataType>& item_type,
                              bool keys_sorted) {
  return std::make_shared<MapType>(key_type, item_type, keys_sorted);
}

std::shared_ptr<DataType> map(const std::shared_ptr<DataType>& key_type,
                              const std::shared_ptr<Field>& item_field,
                              bool keys_sorted) {
  return std::make_shared<MapType>(key_type, item_field, keys_sorted);
}

std::shared_ptr<DataType> fixed_size_list(const std::shared_ptr<DataType>& value_type,
                                          int32_t list_size) {
  return std::make_shared<FixedSizeListType>(value_type, list_size);
}

std::shared_ptr<DataType> fixed_size_list(const std::shared_ptr<Field>& value_field,
                                          int32_t list_size) {
  return std::make_shared<FixedSizeListType>(value_field, list_size);
}

std::shared_ptr<DataType> struct_(const std::vector<std::shared_ptr<Field>>& fields) {
  return std::make_shared<StructType>(fields);
}

std::shared_ptr<DataType> union_(const std::vector<std::shared_ptr<Field>>& child_fields,
                                 const std::vector<int8_t>& type_codes,
                                 UnionMode::type mode) {
  return std::make_shared<UnionType>(child_fields, type_codes, mode);
}

std::shared_ptr<DataType> union_(const std::vector<std::shared_ptr<Field>>& child_fields,
                                 UnionMode::type mode) {
  std::vector<int8_t> type_codes(child_fields.size());
  for (int i = 0; i < static_cast<int>(child_fields.size()); ++i) {
    type_codes[i] = static_cast<int8_t>(i);
  }
  return std::make_shared<UnionType>(child_fields, type_codes, mode);
}

std::shared_ptr<DataType> union_(UnionMode::type mode) {
  std::vector<std::shared_ptr<Field>> child_fields;
  return union_(child_fields, mode);
}

std::shared_ptr<DataType> union_(const std::vector<std::shared_ptr<Array>>& children,
                                 const std::vector<std::string>& field_names,
                                 const std::vector<int8_t>& given_type_codes,
                                 UnionMode::type mode) {
  std::vector<std::shared_ptr<Field>> fields;
  std::vector<int8_t> type_codes(given_type_codes);
  int8_t counter = 0;
  for (const auto& child : children) {
    if (field_names.size() == 0) {
      fields.push_back(field(std::to_string(counter), child->type()));
    } else {
      fields.push_back(field(std::move(field_names[counter]), child->type()));
    }
    if (given_type_codes.size() == 0) {
      type_codes.push_back(counter);
    }
    counter++;
  }
  return union_(fields, std::move(type_codes), mode);
}

std::shared_ptr<DataType> dictionary(const std::shared_ptr<DataType>& index_type,
                                     const std::shared_ptr<DataType>& dict_type,
                                     bool ordered) {
  return std::make_shared<DictionaryType>(index_type, dict_type, ordered);
}

std::shared_ptr<Field> field(std::string name, std::shared_ptr<DataType> type,
                             bool nullable,
                             std::shared_ptr<const KeyValueMetadata> metadata) {
  return std::make_shared<Field>(std::move(name), std::move(type), nullable,
                                 std::move(metadata));
}

std::shared_ptr<DataType> decimal(int32_t precision, int32_t scale) {
  return std::make_shared<Decimal128Type>(precision, scale);
}

std::string Decimal128Type::ToString() const {
  std::stringstream s;
  s << "decimal(" << precision_ << ", " << scale_ << ")";
  return s.str();
}

}  // namespace arrow
