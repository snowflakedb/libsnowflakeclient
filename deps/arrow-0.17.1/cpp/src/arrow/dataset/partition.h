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

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "arrow/dataset/filter.h"
#include "arrow/dataset/type_fwd.h"
#include "arrow/dataset/visibility.h"
#include "arrow/util/optional.h"

namespace arrow {

namespace dataset {

// ----------------------------------------------------------------------
// Partitioning

/// \brief Interface for parsing partition expressions from string partition
/// identifiers.
///
/// For example, the identifier "foo=5" might be parsed to an equality expression
/// between the "foo" field and the value 5.
///
/// Some partitionings may store the field names in a metadata
/// store instead of in file paths, for example
/// dataset_root/2009/11/... could be used when the partition fields
/// are "year" and "month"
///
/// Paths are consumed from left to right. Paths must be relative to
/// the root of a partition; path prefixes must be removed before passing
/// the path to a partitioning for parsing.
class ARROW_DS_EXPORT Partitioning {
 public:
  virtual ~Partitioning() = default;

  /// \brief The name identifying the kind of partitioning
  virtual std::string type_name() const = 0;

  /// \brief Parse a path segment into a partition expression
  ///
  /// \param[in] segment the path segment to parse
  /// \param[in] i the index of segment within a path
  /// \return the parsed expression
  virtual Result<std::shared_ptr<Expression>> Parse(const std::string& segment,
                                                    int i) const = 0;

  virtual Result<std::string> Format(const Expression& expr, int i) const {
    // FIXME(bkietz) make this pure virtual
    return Status::NotImplemented("formatting paths from ", type_name(), " Partitioning");
  }

  /// \brief Parse a path into a partition expression
  Result<std::shared_ptr<Expression>> Parse(const std::string& path) const;

  /// \brief A default Partitioning which always yields scalar(true)
  static std::shared_ptr<Partitioning> Default();

  const std::shared_ptr<Schema>& schema() { return schema_; }

 protected:
  explicit Partitioning(std::shared_ptr<Schema> schema) : schema_(std::move(schema)) {}

  std::shared_ptr<Schema> schema_;
};

/// \brief PartitioningFactory provides creation of a partitioning  when the
/// specific schema must be inferred from available paths (no explicit schema is known).
class ARROW_DS_EXPORT PartitioningFactory {
 public:
  virtual ~PartitioningFactory() = default;

  /// \brief The name identifying the kind of partitioning
  virtual std::string type_name() const = 0;

  /// Get the schema for the resulting Partitioning.
  virtual Result<std::shared_ptr<Schema>> Inspect(
      const std::vector<util::string_view>& paths) const = 0;

  /// Create a partitioning using the provided schema
  /// (fields may be dropped).
  virtual Result<std::shared_ptr<Partitioning>> Finish(
      const std::shared_ptr<Schema>& schema) const = 0;

  // FIXME(bkietz) Make these pure virtual
  /// Construct a WritePlan for the provided fragments
  virtual Result<WritePlan> MakeWritePlan(FragmentIterator fragments,
                                          const std::shared_ptr<Schema>& schema);
  /// Construct a WritePlan for the provided fragments, inferring schema
  virtual Result<WritePlan> MakeWritePlan(FragmentIterator fragments);
};

/// \brief Subclass for representing the default, a partitioning that returns
/// the idempotent "true" partition.
class DefaultPartitioning : public Partitioning {
 public:
  DefaultPartitioning() : Partitioning(::arrow::schema({})) {}

  std::string type_name() const override { return "default"; }

  Result<std::shared_ptr<Expression>> Parse(const std::string& segment,
                                            int i) const override {
    return scalar(true);
  }
};

/// \brief Subclass for looking up partition information from a dictionary
/// mapping segments to expressions provided on construction.
class ARROW_DS_EXPORT SegmentDictionaryPartitioning : public Partitioning {
 public:
  SegmentDictionaryPartitioning(
      std::shared_ptr<Schema> schema,
      std::vector<std::unordered_map<std::string, std::shared_ptr<Expression>>>
          dictionaries)
      : Partitioning(std::move(schema)), dictionaries_(std::move(dictionaries)) {}

  std::string type_name() const override { return "segment_dictionary"; }

  /// Return dictionaries_[i][segment] or scalar(true)
  Result<std::shared_ptr<Expression>> Parse(const std::string& segment,
                                            int i) const override;

 protected:
  std::vector<std::unordered_map<std::string, std::shared_ptr<Expression>>> dictionaries_;
};

/// \brief Subclass for the common case of a partitioning which yields an equality
/// expression for each segment
class ARROW_DS_EXPORT KeyValuePartitioning : public Partitioning {
 public:
  /// An unconverted equality expression consisting of a field name and the representation
  /// of a scalar value
  struct Key {
    std::string name, value;
  };

  static Status VisitKeys(
      const Expression& expr,
      const std::function<Status(const std::string& name,
                                 const std::shared_ptr<Scalar>& value)>& visitor);

  static Status SetDefaultValuesFromKeys(const Expression& expr,
                                         RecordBatchProjector* projector);

  /// Convert a Key to a full expression.
  /// If the field referenced in key is absent from the schema will be ignored.
  static Result<std::shared_ptr<Expression>> ConvertKey(const Key& key,
                                                        const Schema& schema);

  /// Extract a partition key from a path segment.
  virtual util::optional<Key> ParseKey(const std::string& segment, int i) const = 0;

  virtual Result<std::string> FormatKey(const Key& expr, int i) const = 0;

  Result<std::shared_ptr<Expression>> Parse(const std::string& segment,
                                            int i) const override;

  Result<std::string> Format(const Expression& expr, int i) const override;

 protected:
  using Partitioning::Partitioning;
};

/// \brief DirectoryPartitioning parses one segment of a path for each field in its
/// schema. All fields are required, so paths passed to DirectoryPartitioning::Parse
/// must contain segments for each field.
///
/// For example given schema<year:int16, month:int8> the path "/2009/11" would be
/// parsed to ("year"_ == 2009 and "month"_ == 11)
class ARROW_DS_EXPORT DirectoryPartitioning : public KeyValuePartitioning {
 public:
  explicit DirectoryPartitioning(std::shared_ptr<Schema> schema)
      : KeyValuePartitioning(std::move(schema)) {}

  std::string type_name() const override { return "schema"; }

  util::optional<Key> ParseKey(const std::string& segment, int i) const override;

  Result<std::string> FormatKey(const Key& key, int i) const override;

  static std::shared_ptr<PartitioningFactory> MakeFactory(
      std::vector<std::string> field_names);
};

/// \brief Multi-level, directory based partitioning
/// originating from Apache Hive with all data files stored in the
/// leaf directories. Data is partitioned by static values of a
/// particular column in the schema. Partition keys are represented in
/// the form $key=$value in directory names.
/// Field order is ignored, as are missing or unrecognized field names.
///
/// For example given schema<year:int16, month:int8, day:int8> the path
/// "/day=321/ignored=3.4/year=2009" parses to ("year"_ == 2009 and "day"_ == 321)
class ARROW_DS_EXPORT HivePartitioning : public KeyValuePartitioning {
 public:
  explicit HivePartitioning(std::shared_ptr<Schema> schema)
      : KeyValuePartitioning(std::move(schema)) {}

  std::string type_name() const override { return "hive"; }

  util::optional<Key> ParseKey(const std::string& segment, int i) const override {
    return ParseKey(segment);
  }

  Result<std::string> FormatKey(const Key& key, int i) const override;

  static util::optional<Key> ParseKey(const std::string& segment);

  static std::shared_ptr<PartitioningFactory> MakeFactory();
};

/// \brief Implementation provided by lambda or other callable
class ARROW_DS_EXPORT FunctionPartitioning : public Partitioning {
 public:
  explicit FunctionPartitioning(
      std::shared_ptr<Schema> schema,
      std::function<Result<std::shared_ptr<Expression>>(const std::string&, int)> impl,
      std::string name = "function")
      : Partitioning(std::move(schema)), impl_(std::move(impl)), name_(std::move(name)) {}

  std::string type_name() const override { return name_; }

  Result<std::shared_ptr<Expression>> Parse(const std::string& segment,
                                            int i) const override {
    return impl_(segment, i);
  }

 private:
  std::function<Result<std::shared_ptr<Expression>>(const std::string&, int)> impl_;
  std::string name_;
};

// TODO(bkietz) use RE2 and named groups to provide RegexpPartitioning

/// \brief Either a Partitioning or a PartitioningFactory
class ARROW_DS_EXPORT PartitioningOrFactory {
 public:
  explicit PartitioningOrFactory(std::shared_ptr<Partitioning> partitioning)
      : variant_(std::move(partitioning)) {}

  explicit PartitioningOrFactory(std::shared_ptr<PartitioningFactory> factory)
      : variant_(std::move(factory)) {}

  PartitioningOrFactory& operator=(std::shared_ptr<Partitioning> partitioning) {
    variant_ = std::move(partitioning);
    return *this;
  }

  PartitioningOrFactory& operator=(std::shared_ptr<PartitioningFactory> factory) {
    variant_ = std::move(factory);
    return *this;
  }

  std::shared_ptr<Partitioning> partitioning() const {
    if (util::holds_alternative<std::shared_ptr<Partitioning>>(variant_)) {
      return util::get<std::shared_ptr<Partitioning>>(variant_);
    }
    return NULLPTR;
  }

  std::shared_ptr<PartitioningFactory> factory() const {
    if (util::holds_alternative<std::shared_ptr<PartitioningFactory>>(variant_)) {
      return util::get<std::shared_ptr<PartitioningFactory>>(variant_);
    }
    return NULLPTR;
  }

 private:
  util::variant<std::shared_ptr<PartitioningFactory>, std::shared_ptr<Partitioning>>
      variant_;
};

}  // namespace dataset
}  // namespace arrow
