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

#include "./arrow_types.h"
#if defined(ARROW_R_WITH_ARROW)

// FileInfo

// [[arrow::export]]
fs::FileType fs___FileInfo__type(const std::shared_ptr<fs::FileInfo>& x) {
  return x->type();
}

// [[arrow::export]]
void fs___FileInfo__set_type(const std::shared_ptr<fs::FileInfo>& x, fs::FileType type) {
  x->set_type(type);
}

// [[arrow::export]]
std::string fs___FileInfo__path(const std::shared_ptr<fs::FileInfo>& x) {
  return x->path();
}

// [[arrow::export]]
void fs___FileInfo__set_path(const std::shared_ptr<fs::FileInfo>& x,
                             const std::string& path) {
  x->set_path(path);
}

// [[arrow::export]]
int64_t fs___FileInfo__size(const std::shared_ptr<fs::FileInfo>& x) { return x->size(); }

// [[arrow::export]]
void fs___FileInfo__set_size(const std::shared_ptr<fs::FileInfo>& x, int64_t size) {
  x->set_size(size);
}

// [[arrow::export]]
std::string fs___FileInfo__base_name(const std::shared_ptr<fs::FileInfo>& x) {
  return x->base_name();
}

// [[arrow::export]]
std::string fs___FileInfo__extension(const std::shared_ptr<fs::FileInfo>& x) {
  return x->extension();
}

// [[arrow::export]]
SEXP fs___FileInfo__mtime(const std::shared_ptr<fs::FileInfo>& x) {
  SEXP res = PROTECT(Rf_allocVector(REALSXP, 1));
  // .mtime() gets us nanoseconds since epoch, POSIXct is seconds since epoch as a double
  REAL(res)[0] = static_cast<double>(x->mtime().time_since_epoch().count()) / 1000000000;
  Rf_classgets(res, arrow::r::data::classes_POSIXct);
  UNPROTECT(1);
  return res;
}

// [[arrow::export]]
void fs___FileInfo__set_mtime(const std::shared_ptr<fs::FileInfo>& x, SEXP time) {
  auto nanosecs =
      std::chrono::nanoseconds(static_cast<int64_t>(REAL(time)[0] * 1000000000));
  x->set_mtime(fs::TimePoint(nanosecs));
}

// Selector

// [[arrow::export]]
std::string fs___FileSelector__base_dir(
    const std::shared_ptr<fs::FileSelector>& selector) {
  return selector->base_dir;
}

// [[arrow::export]]
bool fs___FileSelector__allow_not_found(
    const std::shared_ptr<fs::FileSelector>& selector) {
  return selector->allow_not_found;
}

// [[arrow::export]]
bool fs___FileSelector__recursive(const std::shared_ptr<fs::FileSelector>& selector) {
  return selector->recursive;
}

// [[arrow::export]]
std::shared_ptr<fs::FileSelector> fs___FileSelector__create(const std::string& base_dir,
                                                            bool allow_not_found,
                                                            bool recursive) {
  auto selector = std::make_shared<fs::FileSelector>();
  selector->base_dir = base_dir;
  selector->allow_not_found = allow_not_found;
  selector->recursive = recursive;
  return selector;
}

// FileSystem

template <typename T>
std::vector<std::shared_ptr<T>> shared_ptr_vector(const std::vector<T>& vec) {
  std::vector<std::shared_ptr<fs::FileInfo>> res(vec.size());
  std::transform(vec.begin(), vec.end(), res.begin(),
                 [](const fs::FileInfo& x) { return std::make_shared<fs::FileInfo>(x); });
  return res;
}

// [[arrow::export]]
std::vector<std::shared_ptr<fs::FileInfo>> fs___FileSystem__GetTargetInfos_Paths(
    const std::shared_ptr<fs::FileSystem>& file_system,
    const std::vector<std::string>& paths) {
  auto results = VALUE_OR_STOP(file_system->GetFileInfo(paths));
  return shared_ptr_vector(results);
}

// [[arrow::export]]
std::vector<std::shared_ptr<fs::FileInfo>> fs___FileSystem__GetTargetInfos_FileSelector(
    const std::shared_ptr<fs::FileSystem>& file_system,
    const std::shared_ptr<fs::FileSelector>& selector) {
  auto results = VALUE_OR_STOP(file_system->GetFileInfo(*selector));
  return shared_ptr_vector(results);
}

// [[arrow::export]]
void fs___FileSystem__CreateDir(const std::shared_ptr<fs::FileSystem>& file_system,
                                const std::string& path, bool recursive) {
  STOP_IF_NOT_OK(file_system->CreateDir(path, recursive));
}

// [[arrow::export]]
void fs___FileSystem__DeleteDir(const std::shared_ptr<fs::FileSystem>& file_system,
                                const std::string& path) {
  STOP_IF_NOT_OK(file_system->DeleteDir(path));
}

// [[arrow::export]]
void fs___FileSystem__DeleteDirContents(
    const std::shared_ptr<fs::FileSystem>& file_system, const std::string& path) {
  STOP_IF_NOT_OK(file_system->DeleteDirContents(path));
}

// [[arrow::export]]
void fs___FileSystem__DeleteFile(const std::shared_ptr<fs::FileSystem>& file_system,
                                 const std::string& path) {
  STOP_IF_NOT_OK(file_system->DeleteFile(path));
}

// [[arrow::export]]
void fs___FileSystem__DeleteFiles(const std::shared_ptr<fs::FileSystem>& file_system,
                                  const std::vector<std::string>& paths) {
  STOP_IF_NOT_OK(file_system->DeleteFiles(paths));
}

// [[arrow::export]]
void fs___FileSystem__Move(const std::shared_ptr<fs::FileSystem>& file_system,
                           const std::string& src, const std::string& dest) {
  STOP_IF_NOT_OK(file_system->Move(src, dest));
}

// [[arrow::export]]
void fs___FileSystem__CopyFile(const std::shared_ptr<fs::FileSystem>& file_system,
                               const std::string& src, const std::string& dest) {
  STOP_IF_NOT_OK(file_system->CopyFile(src, dest));
}

// [[arrow::export]]
std::shared_ptr<arrow::io::InputStream> fs___FileSystem__OpenInputStream(
    const std::shared_ptr<fs::FileSystem>& file_system, const std::string& path) {
  return VALUE_OR_STOP(file_system->OpenInputStream(path));
}

// [[arrow::export]]
std::shared_ptr<arrow::io::RandomAccessFile> fs___FileSystem__OpenInputFile(
    const std::shared_ptr<fs::FileSystem>& file_system, const std::string& path) {
  return VALUE_OR_STOP(file_system->OpenInputFile(path));
}

// [[arrow::export]]
std::shared_ptr<arrow::io::OutputStream> fs___FileSystem__OpenOutputStream(
    const std::shared_ptr<fs::FileSystem>& file_system, const std::string& path) {
  return VALUE_OR_STOP(file_system->OpenOutputStream(path));
}

// [[arrow::export]]
std::shared_ptr<arrow::io::OutputStream> fs___FileSystem__OpenAppendStream(
    const std::shared_ptr<fs::FileSystem>& file_system, const std::string& path) {
  return VALUE_OR_STOP(file_system->OpenAppendStream(path));
}

// [[arrow::export]]
std::shared_ptr<fs::LocalFileSystem> fs___LocalFileSystem__create() {
  return std::make_shared<fs::LocalFileSystem>();
}

// [[arrow::export]]
std::shared_ptr<fs::SubTreeFileSystem> fs___SubTreeFileSystem__create(
    const std::string& base_path, const std::shared_ptr<fs::FileSystem>& base_fs) {
  return std::make_shared<fs::SubTreeFileSystem>(base_path, base_fs);
}

#endif
