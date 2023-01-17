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

#include "arrow/io/interfaces.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <typeinfo>
#include <utility>

#include "arrow/buffer.h"
#include "arrow/io/concurrency.h"
#include "arrow/io/util_internal.h"
#include "arrow/result.h"
#include "arrow/status.h"
#include "arrow/util/future.h"
#include "arrow/util/iterator.h"
#include "arrow/util/logging.h"
#include "arrow/util/string_view.h"
#include "arrow/util/thread_pool.h"

namespace arrow {

using internal::ThreadPool;

namespace io {

FileInterface::~FileInterface() = default;

Status FileInterface::Abort() { return Close(); }

class InputStreamBlockIterator {
 public:
  InputStreamBlockIterator(std::shared_ptr<InputStream> stream, int64_t block_size)
      : stream_(stream), block_size_(block_size) {}

  Result<std::shared_ptr<Buffer>> Next() {
    if (done_) {
      return nullptr;
    }

    ARROW_ASSIGN_OR_RAISE(auto out, stream_->Read(block_size_));

    if (out->size() == 0) {
      done_ = true;
      stream_.reset();
      out.reset();
    }

    return out;
  }

 protected:
  std::shared_ptr<InputStream> stream_;
  int64_t block_size_;
  bool done_ = false;
};

Status InputStream::Advance(int64_t nbytes) { return Read(nbytes).status(); }

Result<util::string_view> InputStream::Peek(int64_t ARROW_ARG_UNUSED(nbytes)) {
  return Status::NotImplemented("Peek not implemented");
}

bool InputStream::supports_zero_copy() const { return false; }

Result<Iterator<std::shared_ptr<Buffer>>> MakeInputStreamIterator(
    std::shared_ptr<InputStream> stream, int64_t block_size) {
  if (stream->closed()) {
    return Status::Invalid("Cannot take iterator on closed stream");
  }
  DCHECK_GT(block_size, 0);
  return Iterator<std::shared_ptr<Buffer>>(InputStreamBlockIterator(stream, block_size));
}

struct RandomAccessFile::RandomAccessFileImpl {
  std::mutex lock_;
};

RandomAccessFile::~RandomAccessFile() = default;

RandomAccessFile::RandomAccessFile()
    : interface_impl_(new RandomAccessFile::RandomAccessFileImpl()) {}

Result<int64_t> RandomAccessFile::ReadAt(int64_t position, int64_t nbytes, void* out) {
  std::lock_guard<std::mutex> lock(interface_impl_->lock_);
  RETURN_NOT_OK(Seek(position));
  return Read(nbytes, out);
}

Result<std::shared_ptr<Buffer>> RandomAccessFile::ReadAt(int64_t position,
                                                         int64_t nbytes) {
  std::lock_guard<std::mutex> lock(interface_impl_->lock_);
  RETURN_NOT_OK(Seek(position));
  return Read(nbytes);
}

// Default ReadAsync() implementation: simply issue the read on one of the IO threads
Future<std::shared_ptr<Buffer>> RandomAccessFile::ReadAsync(int64_t position,
                                                            int64_t nbytes) {
  auto pool = internal::GetIOThreadPool();
  auto self = shared_from_this();
  auto maybe_fut =
      pool->Submit([self, position, nbytes] { return self->ReadAt(position, nbytes); });
  if (!maybe_fut.ok()) {
    return Future<std::shared_ptr<Buffer>>::MakeFinished(maybe_fut.status());
  }
  return *std::move(maybe_fut);
}

Status Writable::Write(const std::string& data) {
  return Write(data.c_str(), static_cast<int64_t>(data.size()));
}

Status Writable::Write(const std::shared_ptr<Buffer>& data) {
  return Write(data->data(), data->size());
}

Status Writable::Flush() { return Status::OK(); }

// An InputStream that reads from a delimited range of a RandomAccessFile
class FileSegmentReader
    : public internal::InputStreamConcurrencyWrapper<FileSegmentReader> {
 public:
  FileSegmentReader(std::shared_ptr<RandomAccessFile> file, int64_t file_offset,
                    int64_t nbytes)
      : file_(std::move(file)),
        closed_(false),
        position_(0),
        file_offset_(file_offset),
        nbytes_(nbytes) {
    FileInterface::set_mode(FileMode::READ);
  }

  Status CheckOpen() const {
    if (closed_) {
      return Status::IOError("Stream is closed");
    }
    return Status::OK();
  }

  Status DoClose() {
    closed_ = true;
    return Status::OK();
  }

  Result<int64_t> DoTell() const {
    RETURN_NOT_OK(CheckOpen());
    return position_;
  }

  bool closed() const override { return closed_; }

  Result<int64_t> DoRead(int64_t nbytes, void* out) {
    RETURN_NOT_OK(CheckOpen());
    int64_t bytes_to_read = std::min(nbytes, nbytes_ - position_);
    ARROW_ASSIGN_OR_RAISE(int64_t bytes_read,
                          file_->ReadAt(file_offset_ + position_, bytes_to_read, out));
    position_ += bytes_read;
    return bytes_read;
  }

  Result<std::shared_ptr<Buffer>> DoRead(int64_t nbytes) {
    RETURN_NOT_OK(CheckOpen());
    int64_t bytes_to_read = std::min(nbytes, nbytes_ - position_);
    ARROW_ASSIGN_OR_RAISE(auto buffer,
                          file_->ReadAt(file_offset_ + position_, bytes_to_read));
    position_ += buffer->size();
    return buffer;
  }

 private:
  std::shared_ptr<RandomAccessFile> file_;
  bool closed_;
  int64_t position_;
  int64_t file_offset_;
  int64_t nbytes_;
};

std::shared_ptr<InputStream> RandomAccessFile::GetStream(
    std::shared_ptr<RandomAccessFile> file, int64_t file_offset, int64_t nbytes) {
  return std::make_shared<FileSegmentReader>(std::move(file), file_offset, nbytes);
}

// -----------------------------------------------------------------------
// Implement utilities exported from concurrency.h and util_internal.h

namespace internal {

void CloseFromDestructor(FileInterface* file) {
  Status st = file->Close();
  if (!st.ok()) {
    auto file_type = typeid(*file).name();
#ifdef NDEBUG
    ARROW_LOG(ERROR) << "Error ignored when destroying file of type " << file_type << ": "
                     << st;
#else
    std::stringstream ss;
    ss << "When destroying file of type " << file_type << ": " << st.message();
    ARROW_LOG(FATAL) << st.WithMessage(ss.str());
#endif
  }
}

Result<int64_t> ValidateReadRange(int64_t offset, int64_t size, int64_t file_size) {
  if (offset < 0 || size < 0) {
    return Status::Invalid("Invalid read (offset = ", offset, ", size = ", size, ")");
  }
  if (offset > file_size) {
    return Status::IOError("Read out of bounds (offset = ", offset, ", size = ", size,
                           ") in file of size ", file_size);
  }
  return std::min(size, file_size - offset);
}

Status ValidateWriteRange(int64_t offset, int64_t size, int64_t file_size) {
  if (offset < 0 || size < 0) {
    return Status::Invalid("Invalid write (offset = ", offset, ", size = ", size, ")");
  }
  if (offset + size > file_size) {
    return Status::IOError("Write out of bounds (offset = ", offset, ", size = ", size,
                           ") in file of size ", file_size);
  }
  return Status::OK();
}

Status ValidateRange(int64_t offset, int64_t size) {
  if (offset < 0 || size < 0) {
    return Status::Invalid("Invalid IO (offset = ", offset, ", size = ", size, ")");
  }
  return Status::OK();
}

#ifndef NDEBUG

// Debug mode concurrency checking

struct SharedExclusiveChecker::Impl {
  std::mutex mutex;
  int64_t n_shared = 0;
  int64_t n_exclusive = 0;
};

SharedExclusiveChecker::SharedExclusiveChecker() : impl_(new Impl) {}

void SharedExclusiveChecker::LockShared() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  // XXX The error message doesn't really describe the actual situation
  // (e.g. ReadAt() called while Read() call in progress)
  ARROW_CHECK_EQ(impl_->n_exclusive, 0)
      << "Attempted to take shared lock while locked exclusive";
  ++impl_->n_shared;
}

void SharedExclusiveChecker::UnlockShared() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  ARROW_CHECK_GT(impl_->n_shared, 0);
  --impl_->n_shared;
}

void SharedExclusiveChecker::LockExclusive() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  ARROW_CHECK_EQ(impl_->n_shared, 0)
      << "Attempted to take exclusive lock while locked shared";
  ARROW_CHECK_EQ(impl_->n_exclusive, 0)
      << "Attempted to take exclusive lock while already locked exclusive";
  ++impl_->n_exclusive;
}

void SharedExclusiveChecker::UnlockExclusive() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  ARROW_CHECK_EQ(impl_->n_exclusive, 1);
  --impl_->n_exclusive;
}

#else

// Release mode no-op concurrency checking

struct SharedExclusiveChecker::Impl {};

SharedExclusiveChecker::SharedExclusiveChecker() {}

void SharedExclusiveChecker::LockShared() {}
void SharedExclusiveChecker::UnlockShared() {}
void SharedExclusiveChecker::LockExclusive() {}
void SharedExclusiveChecker::UnlockExclusive() {}

#endif

static std::shared_ptr<ThreadPool> MakeIOThreadPool() {
  auto maybe_pool = ThreadPool::MakeEternal(/*threads=*/8);
  if (!maybe_pool.ok()) {
    maybe_pool.status().Abort("Failed to create global IO thread pool");
  }
  return *std::move(maybe_pool);
}

ThreadPool* GetIOThreadPool() {
  static std::shared_ptr<ThreadPool> pool = MakeIOThreadPool();
  return pool.get();
}

// -----------------------------------------------------------------------
// CoalesceReadRanges

namespace {

struct ReadRangeCombiner {
  std::vector<ReadRange> Coalesce(std::vector<ReadRange> ranges) {
    if (ranges.size() == 0) {
      return ranges;
    }

    // Remove zero-sized ranges
    auto end = std::remove_if(ranges.begin(), ranges.end(),
                              [](const ReadRange& range) { return range.length == 0; });
    ranges.resize(end - ranges.begin());
    // Sort in position order
    std::sort(ranges.begin(), ranges.end(),
              [](const ReadRange& a, const ReadRange& b) { return a.offset < b.offset; });

#ifndef NDEBUG
    for (size_t i = 0; i < ranges.size() - 1; ++i) {
      const auto& left = ranges[i];
      const auto& right = ranges[i + 1];
      DCHECK_LE(left.offset, right.offset);
      DCHECK_LE(left.offset + left.length, right.offset) << "Some read ranges overlap";
    }
#endif

    std::vector<ReadRange> coalesced;

    // Find some subsets of ranges that we may want to coalesce
    auto start = ranges.begin(), prev = start, next = prev;

    while (++next != ranges.end()) {
      if (next->offset - prev->offset - prev->length > hole_size_limit_) {
        // Distance between consecutive ranges is too large, coalesce this subset
        // and start a new one
        if (next - start > 1) {
          CoalesceUntilLargeEnough(start, next, &coalesced);
        } else {
          coalesced.push_back(*start);
        }
        start = next;
      }
      prev = next;
    }
    // Coalesce last subset
    if (next - start > 1) {
      CoalesceUntilLargeEnough(start, next, &coalesced);
    } else {
      coalesced.push_back(*start);
    }

    DCHECK_EQ(coalesced.front().offset, ranges.front().offset);
    DCHECK_EQ(coalesced.back().offset + coalesced.back().length,
              ranges.back().offset + ranges.back().length);
    return coalesced;
  }

  // Coalesce consecutive pairs of ranges, but only if the resulting range size
  // would not exceed range_size_limit.
  template <typename ReadRangeIterator>
  void CoalesceUntilLargeEnough(ReadRangeIterator begin, ReadRangeIterator end,
                                std::vector<ReadRange>* out) {
    std::list<ReadRange> todo;
    std::copy(begin, end, std::back_inserter(todo));

    // Iterate over consecutive pairs
    auto prev = todo.begin(), next = prev;
    while (++next != todo.end()) {
      DCHECK_GE(next->offset, prev->offset);
      if (CanCoalesce(*prev, *next)) {
        next->length = (next->offset - prev->offset) + next->length;
        next->offset = prev->offset;
        todo.erase(prev);  // Keep `next` valid
      }
      prev = next;
    }

    const auto out_size = out->size();
    out->resize(out_size + todo.size());
    std::copy(todo.begin(), todo.end(), &(*out)[out_size]);
  }

  bool CanCoalesce(const ReadRange& left, const ReadRange& right) {
    DCHECK_LE(left.offset, right.offset);
    // Ensured by the subset-finding in Coalesce()
    DCHECK_LE(right.offset - left.offset - left.length, hole_size_limit_);
    return left.length + right.length <= range_size_limit_;
  }

  const int64_t hole_size_limit_;
  const int64_t range_size_limit_;
};

};  // namespace

std::vector<ReadRange> CoalesceReadRanges(std::vector<ReadRange> ranges,
                                          int64_t hole_size_limit,
                                          int64_t range_size_limit) {
  DCHECK_GT(range_size_limit, hole_size_limit);

  ReadRangeCombiner combiner{hole_size_limit, range_size_limit};
  return combiner.Coalesce(std::move(ranges));
}

}  // namespace internal
}  // namespace io
}  // namespace arrow
