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

#include "arrow/ipc/json_integration.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "arrow/buffer.h"
#include "arrow/io/file.h"
#include "arrow/ipc/dictionary.h"
#include "arrow/ipc/json_internal.h"
#include "arrow/record_batch.h"
#include "arrow/result.h"
#include "arrow/status.h"
#include "arrow/type.h"
#include "arrow/util/logging.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

using std::size_t;

namespace arrow {
namespace ipc {
namespace internal {
namespace json {

// ----------------------------------------------------------------------
// Writer implementation

class JsonWriter::JsonWriterImpl {
 public:
  explicit JsonWriterImpl(const std::shared_ptr<Schema>& schema)
      : schema_(schema), first_batch_written_(false) {
    writer_.reset(new RjWriter(string_buffer_));
  }

  Status Start() {
    writer_->StartObject();
    RETURN_NOT_OK(json::WriteSchema(*schema_, &dictionary_memo_, writer_.get()));
    return Status::OK();
  }

  Status FirstRecordBatch(const RecordBatch& batch) {
    RETURN_NOT_OK(CollectDictionaries(batch, &dictionary_memo_));

    // Write dictionaries, if any
    if (dictionary_memo_.num_dictionaries() > 0) {
      writer_->Key("dictionaries");
      writer_->StartArray();
      for (const auto& entry : dictionary_memo_.id_to_dictionary()) {
        RETURN_NOT_OK(WriteDictionary(entry.first, entry.second, writer_.get()));
      }
      writer_->EndArray();
    }

    // Record batches
    writer_->Key("batches");
    writer_->StartArray();
    first_batch_written_ = true;
    return Status::OK();
  }

  Status Finish(std::string* result) {
    writer_->EndArray();  // Record batches
    writer_->EndObject();

    *result = string_buffer_.GetString();
    return Status::OK();
  }

  Status WriteRecordBatch(const RecordBatch& batch) {
    DCHECK_EQ(batch.num_columns(), schema_->num_fields());

    if (!first_batch_written_) {
      RETURN_NOT_OK(FirstRecordBatch(batch));
    }
    return json::WriteRecordBatch(batch, writer_.get());
  }

 private:
  std::shared_ptr<Schema> schema_;
  DictionaryMemo dictionary_memo_;

  bool first_batch_written_;

  rj::StringBuffer string_buffer_;
  std::unique_ptr<RjWriter> writer_;
};

JsonWriter::JsonWriter(const std::shared_ptr<Schema>& schema) {
  impl_.reset(new JsonWriterImpl(schema));
}

JsonWriter::~JsonWriter() {}

Status JsonWriter::Open(const std::shared_ptr<Schema>& schema,
                        std::unique_ptr<JsonWriter>* writer) {
  *writer = std::unique_ptr<JsonWriter>(new JsonWriter(schema));
  return (*writer)->impl_->Start();
}

Status JsonWriter::Finish(std::string* result) { return impl_->Finish(result); }

Status JsonWriter::WriteRecordBatch(const RecordBatch& batch) {
  return impl_->WriteRecordBatch(batch);
}

// ----------------------------------------------------------------------
// Reader implementation

class JsonReader::JsonReaderImpl {
 public:
  JsonReaderImpl(MemoryPool* pool, const std::shared_ptr<Buffer>& data)
      : pool_(pool), data_(data), record_batches_(nullptr) {}

  Status ParseAndReadSchema() {
    doc_.Parse(reinterpret_cast<const rj::Document::Ch*>(data_->data()),
               static_cast<size_t>(data_->size()));
    if (doc_.HasParseError()) {
      return Status::IOError("JSON parsing failed");
    }

    RETURN_NOT_OK(json::ReadSchema(doc_, pool_, &dictionary_memo_, &schema_));

    auto it = doc_.FindMember("batches");
    RETURN_NOT_ARRAY("batches", it, doc_);
    record_batches_ = &it->value;

    return Status::OK();
  }

  Status ReadRecordBatch(int i, std::shared_ptr<RecordBatch>* batch) {
    DCHECK_GE(i, 0) << "i out of bounds";
    DCHECK_LT(i, static_cast<int>(record_batches_->GetArray().Size()))
        << "i out of bounds";

    return json::ReadRecordBatch(record_batches_->GetArray()[i], schema_,
                                 &dictionary_memo_, pool_, batch);
  }

  std::shared_ptr<Schema> schema() const { return schema_; }

  int num_record_batches() const {
    return static_cast<int>(record_batches_->GetArray().Size());
  }

 private:
  MemoryPool* pool_;
  std::shared_ptr<Buffer> data_;
  rj::Document doc_;

  const rj::Value* record_batches_;
  std::shared_ptr<Schema> schema_;
  DictionaryMemo dictionary_memo_;
};

JsonReader::JsonReader(MemoryPool* pool, const std::shared_ptr<Buffer>& data) {
  impl_.reset(new JsonReaderImpl(pool, data));
}

JsonReader::~JsonReader() {}

Status JsonReader::Open(const std::shared_ptr<Buffer>& data,
                        std::unique_ptr<JsonReader>* reader) {
  return Open(default_memory_pool(), data, reader);
}

Status JsonReader::Open(MemoryPool* pool, const std::shared_ptr<Buffer>& data,
                        std::unique_ptr<JsonReader>* reader) {
  *reader = std::unique_ptr<JsonReader>(new JsonReader(pool, data));
  return (*reader)->impl_->ParseAndReadSchema();
}

Status JsonReader::Open(MemoryPool* pool,
                        const std::shared_ptr<io::ReadableFile>& in_file,
                        std::unique_ptr<JsonReader>* reader) {
  ARROW_ASSIGN_OR_RAISE(int64_t file_size, in_file->GetSize());
  ARROW_ASSIGN_OR_RAISE(auto json_buffer, in_file->Read(file_size));
  return Open(pool, json_buffer, reader);
}

std::shared_ptr<Schema> JsonReader::schema() const { return impl_->schema(); }

int JsonReader::num_record_batches() const { return impl_->num_record_batches(); }

Status JsonReader::ReadRecordBatch(int i, std::shared_ptr<RecordBatch>* batch) const {
  return impl_->ReadRecordBatch(i, batch);
}

}  // namespace json
}  // namespace internal
}  // namespace ipc
}  // namespace arrow
