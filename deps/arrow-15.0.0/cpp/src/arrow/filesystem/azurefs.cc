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

#include "arrow/filesystem/azurefs.h"
#include "arrow/filesystem/azurefs_internal.h"

// idenfity.hpp triggers -Wattributes warnings cause -Werror builds to fail,
// so disable it for this file with pragmas.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif
#include <azure/identity.hpp>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <azure/storage/blobs.hpp>
#include <azure/storage/files/datalake.hpp>

#include "arrow/buffer.h"
#include "arrow/filesystem/path_util.h"
#include "arrow/filesystem/util_internal.h"
#include "arrow/io/util_internal.h"
#include "arrow/result.h"
#include "arrow/util/checked_cast.h"
#include "arrow/util/formatting.h"
#include "arrow/util/future.h"
#include "arrow/util/key_value_metadata.h"
#include "arrow/util/logging.h"
#include "arrow/util/string.h"

namespace arrow::fs {

namespace Blobs = Azure::Storage::Blobs;
namespace Core = Azure::Core;
namespace DataLake = Azure::Storage::Files::DataLake;
namespace Http = Azure::Core::Http;
namespace Storage = Azure::Storage;

using HNSSupport = internal::HierarchicalNamespaceSupport;

// -----------------------------------------------------------------------
// AzureOptions Implementation

AzureOptions::AzureOptions() = default;

AzureOptions::~AzureOptions() = default;

bool AzureOptions::Equals(const AzureOptions& other) const {
  // TODO(GH-38598): update here when more auth methods are added.
  const bool equals = blob_storage_authority == other.blob_storage_authority &&
                      dfs_storage_authority == other.dfs_storage_authority &&
                      blob_storage_scheme == other.blob_storage_scheme &&
                      dfs_storage_scheme == other.dfs_storage_scheme &&
                      default_metadata == other.default_metadata &&
                      account_name == other.account_name &&
                      credential_kind_ == other.credential_kind_;
  if (!equals) {
    return false;
  }
  switch (credential_kind_) {
    case CredentialKind::kDefault:
    case CredentialKind::kAnonymous:
      return true;
    case CredentialKind::kStorageSharedKey:
      return storage_shared_key_credential_->AccountName ==
             other.storage_shared_key_credential_->AccountName;
    case CredentialKind::kClientSecret:
    case CredentialKind::kManagedIdentity:
    case CredentialKind::kWorkloadIdentity:
      return token_credential_->GetCredentialName() ==
             other.token_credential_->GetCredentialName();
  }
  DCHECK(false);
  return false;
}

namespace {
std::string BuildBaseUrl(const std::string& scheme, const std::string& authority,
                         const std::string& account_name) {
  std::string url;
  url += scheme + "://";
  if (!authority.empty()) {
    if (authority[0] == '.') {
      url += account_name;
      url += authority;
    } else {
      url += authority;
      url += "/";
      url += account_name;
    }
  }
  url += "/";
  return url;
}
}  // namespace

std::string AzureOptions::AccountBlobUrl(const std::string& account_name) const {
  return BuildBaseUrl(blob_storage_scheme, blob_storage_authority, account_name);
}

std::string AzureOptions::AccountDfsUrl(const std::string& account_name) const {
  return BuildBaseUrl(dfs_storage_scheme, dfs_storage_authority, account_name);
}

Status AzureOptions::ConfigureDefaultCredential() {
  credential_kind_ = CredentialKind::kDefault;
  token_credential_ = std::make_shared<Azure::Identity::DefaultAzureCredential>();
  return Status::OK();
}

Status AzureOptions::ConfigureAnonymousCredential() {
  credential_kind_ = CredentialKind::kAnonymous;
  return Status::OK();
}

Status AzureOptions::ConfigureAccountKeyCredential(const std::string& account_key) {
  credential_kind_ = CredentialKind::kStorageSharedKey;
  if (account_name.empty()) {
    return Status::Invalid("AzureOptions doesn't contain a valid account name");
  }
  storage_shared_key_credential_ =
      std::make_shared<Storage::StorageSharedKeyCredential>(account_name, account_key);
  return Status::OK();
}

Status AzureOptions::ConfigureClientSecretCredential(const std::string& tenant_id,
                                                     const std::string& client_id,
                                                     const std::string& client_secret) {
  credential_kind_ = CredentialKind::kClientSecret;
  token_credential_ = std::make_shared<Azure::Identity::ClientSecretCredential>(
      tenant_id, client_id, client_secret);
  return Status::OK();
}

Status AzureOptions::ConfigureManagedIdentityCredential(const std::string& client_id) {
  credential_kind_ = CredentialKind::kManagedIdentity;
  token_credential_ =
      std::make_shared<Azure::Identity::ManagedIdentityCredential>(client_id);
  return Status::OK();
}

Status AzureOptions::ConfigureWorkloadIdentityCredential() {
  credential_kind_ = CredentialKind::kWorkloadIdentity;
  token_credential_ = std::make_shared<Azure::Identity::WorkloadIdentityCredential>();
  return Status::OK();
}

Result<std::unique_ptr<Blobs::BlobServiceClient>> AzureOptions::MakeBlobServiceClient()
    const {
  if (account_name.empty()) {
    return Status::Invalid("AzureOptions doesn't contain a valid account name");
  }
  switch (credential_kind_) {
    case CredentialKind::kAnonymous:
      return std::make_unique<Blobs::BlobServiceClient>(AccountBlobUrl(account_name));
    case CredentialKind::kDefault:
      if (!token_credential_) {
        token_credential_ = std::make_shared<Azure::Identity::DefaultAzureCredential>();
      }
      [[fallthrough]];
    case CredentialKind::kClientSecret:
    case CredentialKind::kManagedIdentity:
    case CredentialKind::kWorkloadIdentity:
      return std::make_unique<Blobs::BlobServiceClient>(AccountBlobUrl(account_name),
                                                        token_credential_);
    case CredentialKind::kStorageSharedKey:
      return std::make_unique<Blobs::BlobServiceClient>(AccountBlobUrl(account_name),
                                                        storage_shared_key_credential_);
  }
  return Status::Invalid("AzureOptions doesn't contain a valid auth configuration");
}

Result<std::unique_ptr<DataLake::DataLakeServiceClient>>
AzureOptions::MakeDataLakeServiceClient() const {
  if (account_name.empty()) {
    return Status::Invalid("AzureOptions doesn't contain a valid account name");
  }
  switch (credential_kind_) {
    case CredentialKind::kAnonymous:
      return std::make_unique<DataLake::DataLakeServiceClient>(
          AccountDfsUrl(account_name));
    case CredentialKind::kDefault:
      if (!token_credential_) {
        token_credential_ = std::make_shared<Azure::Identity::DefaultAzureCredential>();
      }
      [[fallthrough]];
    case CredentialKind::kClientSecret:
    case CredentialKind::kManagedIdentity:
    case CredentialKind::kWorkloadIdentity:
      return std::make_unique<DataLake::DataLakeServiceClient>(
          AccountDfsUrl(account_name), token_credential_);
    case CredentialKind::kStorageSharedKey:
      return std::make_unique<DataLake::DataLakeServiceClient>(
          AccountDfsUrl(account_name), storage_shared_key_credential_);
  }
  return Status::Invalid("AzureOptions doesn't contain a valid auth configuration");
}

namespace {

// An AzureFileSystem represents an Azure storage account. An AzureLocation describes a
// container in that storage account and a path within that container.
struct AzureLocation {
  std::string all;
  std::string container;
  std::string path;
  std::vector<std::string> path_parts;

  static Result<AzureLocation> FromString(const std::string& string) {
    // Example expected string format: testcontainer/testdir/testfile.txt
    // container = testcontainer
    // path = testdir/testfile.txt
    // path_parts = [testdir, testfile.txt]
    if (internal::IsLikelyUri(string)) {
      return Status::Invalid(
          "Expected an Azure object location of the form 'container/path...',"
          " got a URI: '",
          string, "'");
    }
    auto first_sep = string.find_first_of(internal::kSep);
    if (first_sep == 0) {
      return Status::Invalid("Location cannot start with a separator ('", string, "')");
    }
    if (first_sep == std::string::npos) {
      return AzureLocation{string, string, "", {}};
    }
    AzureLocation location;
    location.all = string;
    location.container = string.substr(0, first_sep);
    location.path = string.substr(first_sep + 1);
    location.path_parts = internal::SplitAbstractPath(location.path);
    RETURN_NOT_OK(location.Validate());
    return location;
  }

  AzureLocation parent() const {
    DCHECK(has_parent());
    AzureLocation parent{"", container, "", path_parts};
    parent.path_parts.pop_back();
    parent.path = internal::JoinAbstractPath(parent.path_parts);
    if (parent.path.empty()) {
      parent.all = parent.container;
    } else {
      parent.all = parent.container + internal::kSep + parent.path;
    }
    return parent;
  }

  Result<AzureLocation> join(const std::string& stem) const {
    return FromString(internal::ConcatAbstractPath(all, stem));
  }

  bool has_parent() const { return !path.empty(); }

  bool empty() const { return container.empty() && path.empty(); }

  bool operator==(const AzureLocation& other) const {
    return container == other.container && path == other.path;
  }

 private:
  Status Validate() {
    auto status = internal::ValidateAbstractPathParts(path_parts);
    return status.ok() ? status : Status::Invalid(status.message(), " in location ", all);
  }
};

template <typename... PrefixArgs>
Status ExceptionToStatus(const Storage::StorageException& exception,
                         PrefixArgs&&... prefix_args) {
  return Status::IOError(std::forward<PrefixArgs>(prefix_args)...,
                         " Azure Error: ", exception.what());
}

Status PathNotFound(const AzureLocation& location) {
  return ::arrow::fs::internal::PathNotFound(location.all);
}

Status NotAFile(const AzureLocation& location) {
  return ::arrow::fs::internal::NotAFile(location.all);
}

Status ValidateFileLocation(const AzureLocation& location) {
  if (location.container.empty()) {
    return PathNotFound(location);
  }
  if (location.path.empty()) {
    return NotAFile(location);
  }
  return internal::AssertNoTrailingSlash(location.path);
}

std::string_view BodyTextView(const Http::RawResponse& raw_response) {
  const auto& body = raw_response.GetBody();
#ifndef NDEBUG
  auto& headers = raw_response.GetHeaders();
  auto content_type = headers.find("Content-Type");
  if (content_type != headers.end()) {
    DCHECK_EQ(std::string_view{content_type->second}.substr(5), "text/");
  }
#endif
  return std::string_view{reinterpret_cast<const char*>(body.data()), body.size()};
}

Status StatusFromErrorResponse(const std::string& url,
                               const Http::RawResponse& raw_response,
                               const std::string& context) {
  // There isn't an Azure specification that response body on error
  // doesn't contain any binary data but we assume it. We hope that
  // error response body has useful information for the error.
  auto body_text = BodyTextView(raw_response);
  return Status::IOError(context, ": ", url, ": ", raw_response.GetReasonPhrase(), " (",
                         static_cast<int>(raw_response.GetStatusCode()),
                         "): ", body_text);
}

bool IsContainerNotFound(const Storage::StorageException& e) {
  if (e.ErrorCode == "ContainerNotFound" ||
      e.ReasonPhrase == "The specified container does not exist." ||
      e.ReasonPhrase == "The specified filesystem does not exist.") {
    DCHECK_EQ(e.StatusCode, Http::HttpStatusCode::NotFound);
    return true;
  }
  return false;
}

template <typename ArrowType>
std::string FormatValue(typename TypeTraits<ArrowType>::CType value) {
  struct StringAppender {
    std::string string;
    Status operator()(std::string_view view) {
      string.append(view.data(), view.size());
      return Status::OK();
    }
  } appender;
  arrow::internal::StringFormatter<ArrowType> formatter;
  ARROW_UNUSED(formatter(value, appender));
  return appender.string;
}

std::shared_ptr<const KeyValueMetadata> PropertiesToMetadata(
    const Blobs::Models::BlobProperties& properties) {
  auto metadata = std::make_shared<KeyValueMetadata>();
  // Not supported yet:
  // * properties.ObjectReplicationSourceProperties
  // * properties.Metadata
  //
  // They may have the same key defined in the following
  // metadata->Append() list. If we have duplicated key in metadata,
  // the first value may be only used by users because
  // KeyValueMetadata::Get() returns the first found value. Note that
  // users can use all values by using KeyValueMetadata::keys() and
  // KeyValueMetadata::values().
  if (properties.ImmutabilityPolicy.HasValue()) {
    metadata->Append("Immutability-Policy-Expires-On",
                     properties.ImmutabilityPolicy.Value().ExpiresOn.ToString());
    metadata->Append("Immutability-Policy-Mode",
                     properties.ImmutabilityPolicy.Value().PolicyMode.ToString());
  }
  metadata->Append("Content-Type", properties.HttpHeaders.ContentType);
  metadata->Append("Content-Encoding", properties.HttpHeaders.ContentEncoding);
  metadata->Append("Content-Language", properties.HttpHeaders.ContentLanguage);
  const auto& content_hash = properties.HttpHeaders.ContentHash.Value;
  metadata->Append("Content-Hash", HexEncode(content_hash.data(), content_hash.size()));
  metadata->Append("Content-Disposition", properties.HttpHeaders.ContentDisposition);
  metadata->Append("Cache-Control", properties.HttpHeaders.CacheControl);
  metadata->Append("Last-Modified", properties.LastModified.ToString());
  metadata->Append("Created-On", properties.CreatedOn.ToString());
  if (properties.ObjectReplicationDestinationPolicyId.HasValue()) {
    metadata->Append("Object-Replication-Destination-Policy-Id",
                     properties.ObjectReplicationDestinationPolicyId.Value());
  }
  metadata->Append("Blob-Type", properties.BlobType.ToString());
  if (properties.CopyCompletedOn.HasValue()) {
    metadata->Append("Copy-Completed-On", properties.CopyCompletedOn.Value().ToString());
  }
  if (properties.CopyStatusDescription.HasValue()) {
    metadata->Append("Copy-Status-Description", properties.CopyStatusDescription.Value());
  }
  if (properties.CopyId.HasValue()) {
    metadata->Append("Copy-Id", properties.CopyId.Value());
  }
  if (properties.CopyProgress.HasValue()) {
    metadata->Append("Copy-Progress", properties.CopyProgress.Value());
  }
  if (properties.CopySource.HasValue()) {
    metadata->Append("Copy-Source", properties.CopySource.Value());
  }
  if (properties.CopyStatus.HasValue()) {
    metadata->Append("Copy-Status", properties.CopyStatus.Value().ToString());
  }
  if (properties.IsIncrementalCopy.HasValue()) {
    metadata->Append("Is-Incremental-Copy",
                     FormatValue<BooleanType>(properties.IsIncrementalCopy.Value()));
  }
  if (properties.IncrementalCopyDestinationSnapshot.HasValue()) {
    metadata->Append("Incremental-Copy-Destination-Snapshot",
                     properties.IncrementalCopyDestinationSnapshot.Value());
  }
  if (properties.LeaseDuration.HasValue()) {
    metadata->Append("Lease-Duration", properties.LeaseDuration.Value().ToString());
  }
  if (properties.LeaseState.HasValue()) {
    metadata->Append("Lease-State", properties.LeaseState.Value().ToString());
  }
  if (properties.LeaseStatus.HasValue()) {
    metadata->Append("Lease-Status", properties.LeaseStatus.Value().ToString());
  }
  metadata->Append("Content-Length", FormatValue<Int64Type>(properties.BlobSize));
  if (properties.ETag.HasValue()) {
    metadata->Append("ETag", properties.ETag.ToString());
  }
  if (properties.SequenceNumber.HasValue()) {
    metadata->Append("Sequence-Number",
                     FormatValue<Int64Type>(properties.SequenceNumber.Value()));
  }
  if (properties.CommittedBlockCount.HasValue()) {
    metadata->Append("Committed-Block-Count",
                     FormatValue<Int32Type>(properties.CommittedBlockCount.Value()));
  }
  metadata->Append("IsServerEncrypted",
                   FormatValue<BooleanType>(properties.IsServerEncrypted));
  if (properties.EncryptionKeySha256.HasValue()) {
    const auto& sha256 = properties.EncryptionKeySha256.Value();
    metadata->Append("Encryption-Key-Sha-256", HexEncode(sha256.data(), sha256.size()));
  }
  if (properties.EncryptionScope.HasValue()) {
    metadata->Append("Encryption-Scope", properties.EncryptionScope.Value());
  }
  if (properties.AccessTier.HasValue()) {
    metadata->Append("Access-Tier", properties.AccessTier.Value().ToString());
  }
  if (properties.IsAccessTierInferred.HasValue()) {
    metadata->Append("Is-Access-Tier-Inferred",
                     FormatValue<BooleanType>(properties.IsAccessTierInferred.Value()));
  }
  if (properties.ArchiveStatus.HasValue()) {
    metadata->Append("Archive-Status", properties.ArchiveStatus.Value().ToString());
  }
  if (properties.AccessTierChangedOn.HasValue()) {
    metadata->Append("Access-Tier-Changed-On",
                     properties.AccessTierChangedOn.Value().ToString());
  }
  if (properties.VersionId.HasValue()) {
    metadata->Append("Version-Id", properties.VersionId.Value());
  }
  if (properties.IsCurrentVersion.HasValue()) {
    metadata->Append("Is-Current-Version",
                     FormatValue<BooleanType>(properties.IsCurrentVersion.Value()));
  }
  if (properties.TagCount.HasValue()) {
    metadata->Append("Tag-Count", FormatValue<Int32Type>(properties.TagCount.Value()));
  }
  if (properties.ExpiresOn.HasValue()) {
    metadata->Append("Expires-On", properties.ExpiresOn.Value().ToString());
  }
  if (properties.IsSealed.HasValue()) {
    metadata->Append("Is-Sealed", FormatValue<BooleanType>(properties.IsSealed.Value()));
  }
  if (properties.RehydratePriority.HasValue()) {
    metadata->Append("Rehydrate-Priority",
                     properties.RehydratePriority.Value().ToString());
  }
  if (properties.LastAccessedOn.HasValue()) {
    metadata->Append("Last-Accessed-On", properties.LastAccessedOn.Value().ToString());
  }
  metadata->Append("Has-Legal-Hold", FormatValue<BooleanType>(properties.HasLegalHold));
  return metadata;
}

Storage::Metadata ArrowMetadataToAzureMetadata(
    const std::shared_ptr<const KeyValueMetadata>& arrow_metadata) {
  Storage::Metadata azure_metadata;
  for (auto key_value : arrow_metadata->sorted_pairs()) {
    azure_metadata[key_value.first] = key_value.second;
  }
  return azure_metadata;
}

class ObjectInputFile final : public io::RandomAccessFile {
 public:
  ObjectInputFile(std::shared_ptr<Blobs::BlobClient> blob_client,
                  const io::IOContext& io_context, AzureLocation location,
                  int64_t size = kNoSize)
      : blob_client_(std::move(blob_client)),
        io_context_(io_context),
        location_(std::move(location)),
        content_length_(size) {}

  Status Init() {
    if (content_length_ != kNoSize) {
      DCHECK_GE(content_length_, 0);
      return Status::OK();
    }
    try {
      auto properties = blob_client_->GetProperties();
      content_length_ = properties.Value.BlobSize;
      metadata_ = PropertiesToMetadata(properties.Value);
      return Status::OK();
    } catch (const Storage::StorageException& exception) {
      if (exception.StatusCode == Http::HttpStatusCode::NotFound) {
        return PathNotFound(location_);
      }
      return ExceptionToStatus(
          exception, "GetProperties failed for '", blob_client_->GetUrl(),
          "'. Cannot initialise an ObjectInputFile without knowing the file size.");
    }
  }

  Status CheckClosed(const char* action) const {
    if (closed_) {
      return Status::Invalid("Cannot ", action, " on closed file.");
    }
    return Status::OK();
  }

  Status CheckPosition(int64_t position, const char* action) const {
    DCHECK_GE(content_length_, 0);
    if (position < 0) {
      return Status::Invalid("Cannot ", action, " from negative position");
    }
    if (position > content_length_) {
      return Status::IOError("Cannot ", action, " past end of file");
    }
    return Status::OK();
  }

  // RandomAccessFile APIs

  Result<std::shared_ptr<const KeyValueMetadata>> ReadMetadata() override {
    return metadata_;
  }

  Future<std::shared_ptr<const KeyValueMetadata>> ReadMetadataAsync(
      const io::IOContext& io_context) override {
    return metadata_;
  }

  Status Close() override {
    blob_client_ = nullptr;
    closed_ = true;
    return Status::OK();
  }

  bool closed() const override { return closed_; }

  Result<int64_t> Tell() const override {
    RETURN_NOT_OK(CheckClosed("tell"));
    return pos_;
  }

  Result<int64_t> GetSize() override {
    RETURN_NOT_OK(CheckClosed("size"));
    return content_length_;
  }

  Status Seek(int64_t position) override {
    RETURN_NOT_OK(CheckClosed("seek"));
    RETURN_NOT_OK(CheckPosition(position, "seek"));

    pos_ = position;
    return Status::OK();
  }

  Result<int64_t> ReadAt(int64_t position, int64_t nbytes, void* out) override {
    RETURN_NOT_OK(CheckClosed("read"));
    RETURN_NOT_OK(CheckPosition(position, "read"));

    nbytes = std::min(nbytes, content_length_ - position);
    if (nbytes == 0) {
      return 0;
    }

    // Read the desired range of bytes
    Http::HttpRange range{position, nbytes};
    Storage::Blobs::DownloadBlobToOptions download_options;
    download_options.Range = range;
    try {
      return blob_client_
          ->DownloadTo(reinterpret_cast<uint8_t*>(out), nbytes, download_options)
          .Value.ContentRange.Length.Value();
    } catch (const Storage::StorageException& exception) {
      return ExceptionToStatus(
          exception, "DownloadTo from '", blob_client_->GetUrl(), "' at position ",
          position, " for ", nbytes,
          " bytes failed. ReadAt failed to read the required byte range.");
    }
  }

  Result<std::shared_ptr<Buffer>> ReadAt(int64_t position, int64_t nbytes) override {
    RETURN_NOT_OK(CheckClosed("read"));
    RETURN_NOT_OK(CheckPosition(position, "read"));

    // No need to allocate more than the remaining number of bytes
    nbytes = std::min(nbytes, content_length_ - position);

    ARROW_ASSIGN_OR_RAISE(auto buffer,
                          AllocateResizableBuffer(nbytes, io_context_.pool()));
    if (nbytes > 0) {
      ARROW_ASSIGN_OR_RAISE(int64_t bytes_read,
                            ReadAt(position, nbytes, buffer->mutable_data()));
      DCHECK_LE(bytes_read, nbytes);
      RETURN_NOT_OK(buffer->Resize(bytes_read));
    }
    return std::move(buffer);
  }

  Result<int64_t> Read(int64_t nbytes, void* out) override {
    ARROW_ASSIGN_OR_RAISE(int64_t bytes_read, ReadAt(pos_, nbytes, out));
    pos_ += bytes_read;
    return bytes_read;
  }

  Result<std::shared_ptr<Buffer>> Read(int64_t nbytes) override {
    ARROW_ASSIGN_OR_RAISE(auto buffer, ReadAt(pos_, nbytes));
    pos_ += buffer->size();
    return std::move(buffer);
  }

 private:
  std::shared_ptr<Blobs::BlobClient> blob_client_;
  const io::IOContext io_context_;
  AzureLocation location_;

  bool closed_ = false;
  int64_t pos_ = 0;
  int64_t content_length_ = kNoSize;
  std::shared_ptr<const KeyValueMetadata> metadata_;
};

Status CreateEmptyBlockBlob(const Blobs::BlockBlobClient& block_blob_client) {
  try {
    block_blob_client.UploadFrom(nullptr, 0);
  } catch (const Storage::StorageException& exception) {
    return ExceptionToStatus(
        exception, "UploadFrom failed for '", block_blob_client.GetUrl(),
        "'. There is no existing blob at this location or the existing blob must be "
        "replaced so ObjectAppendStream must create a new empty block blob.");
  }
  return Status::OK();
}

Result<Blobs::Models::GetBlockListResult> GetBlockList(
    std::shared_ptr<Blobs::BlockBlobClient> block_blob_client) {
  try {
    return block_blob_client->GetBlockList().Value;
  } catch (Storage::StorageException& exception) {
    return ExceptionToStatus(
        exception, "GetBlockList failed for '", block_blob_client->GetUrl(),
        "'. Cannot write to a file without first fetching the existing block list.");
  }
}

Status CommitBlockList(std::shared_ptr<Storage::Blobs::BlockBlobClient> block_blob_client,
                       const std::vector<std::string>& block_ids,
                       const Storage::Metadata& metadata) {
  Blobs::CommitBlockListOptions options;
  options.Metadata = metadata;
  try {
    // CommitBlockList puts all block_ids in the latest element. That means in the case of
    // overlapping block_ids the newly staged block ids will always replace the
    // previously committed blocks.
    // https://learn.microsoft.com/en-us/rest/api/storageservices/put-block-list?tabs=microsoft-entra-id#request-body
    block_blob_client->CommitBlockList(block_ids, options);
  } catch (const Storage::StorageException& exception) {
    return ExceptionToStatus(
        exception, "CommitBlockList failed for '", block_blob_client->GetUrl(),
        "'. Committing is required to flush an output/append stream.");
  }
  return Status::OK();
}

class ObjectAppendStream final : public io::OutputStream {
 public:
  ObjectAppendStream(std::shared_ptr<Blobs::BlockBlobClient> block_blob_client,
                     const io::IOContext& io_context, const AzureLocation& location,
                     const std::shared_ptr<const KeyValueMetadata>& metadata,
                     const AzureOptions& options, int64_t size = kNoSize)
      : block_blob_client_(std::move(block_blob_client)),
        io_context_(io_context),
        location_(location),
        content_length_(size) {
    if (metadata && metadata->size() != 0) {
      metadata_ = ArrowMetadataToAzureMetadata(metadata);
    } else if (options.default_metadata && options.default_metadata->size() != 0) {
      metadata_ = ArrowMetadataToAzureMetadata(options.default_metadata);
    }
  }

  ~ObjectAppendStream() override {
    // For compliance with the rest of the IO stack, Close rather than Abort,
    // even though it may be more expensive.
    io::internal::CloseFromDestructor(this);
  }

  Status Init() {
    if (content_length_ != kNoSize) {
      DCHECK_GE(content_length_, 0);
      pos_ = content_length_;
    } else {
      try {
        auto properties = block_blob_client_->GetProperties();
        content_length_ = properties.Value.BlobSize;
        pos_ = content_length_;
      } catch (const Storage::StorageException& exception) {
        if (exception.StatusCode == Http::HttpStatusCode::NotFound) {
          RETURN_NOT_OK(CreateEmptyBlockBlob(*block_blob_client_));
        } else {
          return ExceptionToStatus(
              exception, "GetProperties failed for '", block_blob_client_->GetUrl(),
              "'. Cannot initialise an ObjectAppendStream without knowing whether a "
              "file already exists at this path, and if it exists, its size.");
        }
        content_length_ = 0;
      }
    }
    if (content_length_ > 0) {
      ARROW_ASSIGN_OR_RAISE(auto block_list, GetBlockList(block_blob_client_));
      for (auto block : block_list.CommittedBlocks) {
        block_ids_.push_back(block.Name);
      }
    }
    return Status::OK();
  }

  Status Abort() override {
    if (closed_) {
      return Status::OK();
    }
    block_blob_client_ = nullptr;
    closed_ = true;
    return Status::OK();
  }

  Status Close() override {
    if (closed_) {
      return Status::OK();
    }
    RETURN_NOT_OK(Flush());
    block_blob_client_ = nullptr;
    closed_ = true;
    return Status::OK();
  }

  bool closed() const override { return closed_; }

  Status CheckClosed(const char* action) const {
    if (closed_) {
      return Status::Invalid("Cannot ", action, " on closed stream.");
    }
    return Status::OK();
  }

  Result<int64_t> Tell() const override {
    RETURN_NOT_OK(CheckClosed("tell"));
    return pos_;
  }

  Status Write(const std::shared_ptr<Buffer>& buffer) override {
    return DoAppend(buffer->data(), buffer->size(), buffer);
  }

  Status Write(const void* data, int64_t nbytes) override {
    return DoAppend(data, nbytes);
  }

  Status Flush() override {
    RETURN_NOT_OK(CheckClosed("flush"));
    return CommitBlockList(block_blob_client_, block_ids_, metadata_);
  }

 private:
  Status DoAppend(const void* data, int64_t nbytes,
                  std::shared_ptr<Buffer> owned_buffer = nullptr) {
    RETURN_NOT_OK(CheckClosed("append"));
    auto append_data = reinterpret_cast<const uint8_t*>(data);
    Core::IO::MemoryBodyStream block_content(append_data, nbytes);
    if (block_content.Length() == 0) {
      return Status::OK();
    }

    const auto n_block_ids = block_ids_.size();

    // New block ID must always be distinct from the existing block IDs. Otherwise we
    // will accidentally replace the content of existing blocks, causing corruption.
    // We will use monotonically increasing integers.
    auto new_block_id = std::to_string(n_block_ids);

    // Pad to 5 digits, because Azure allows a maximum of 50,000 blocks.
    const size_t target_number_of_digits = 5;
    const auto required_padding_digits =
        target_number_of_digits - std::min(target_number_of_digits, new_block_id.size());
    new_block_id.insert(0, required_padding_digits, '0');
    // There is a small risk when appending to a blob created by another client that
    // `new_block_id` may overlapping with an existing block id. Adding the `-arrow`
    // suffix significantly reduces the risk, but does not 100% eliminate it. For example
    // if the blob was previously created with one block, with id `00001-arrow` then the
    // next block we append will conflict with that, and cause corruption.
    new_block_id += "-arrow";
    new_block_id = Core::Convert::Base64Encode(
        std::vector<uint8_t>(new_block_id.begin(), new_block_id.end()));

    try {
      block_blob_client_->StageBlock(new_block_id, block_content);
    } catch (const Storage::StorageException& exception) {
      return ExceptionToStatus(
          exception, "StageBlock failed for '", block_blob_client_->GetUrl(),
          "' new_block_id: '", new_block_id,
          "'. Staging new blocks is fundamental to streaming writes to blob storage.");
    }
    block_ids_.push_back(new_block_id);
    pos_ += nbytes;
    content_length_ += nbytes;
    return Status::OK();
  }

  std::shared_ptr<Blobs::BlockBlobClient> block_blob_client_;
  const io::IOContext io_context_;
  const AzureLocation location_;

  bool closed_ = false;
  int64_t pos_ = 0;
  int64_t content_length_ = kNoSize;
  std::vector<std::string> block_ids_;
  Storage::Metadata metadata_;
};

bool IsDfsEmulator(const AzureOptions& options) {
  return options.dfs_storage_authority != ".dfs.core.windows.net";
}

}  // namespace

// -----------------------------------------------------------------------
// internal implementation

namespace internal {

Result<HNSSupport> CheckIfHierarchicalNamespaceIsEnabled(
    const DataLake::DataLakeFileSystemClient& adlfs_client, const AzureOptions& options) {
  try {
    auto directory_client = adlfs_client.GetDirectoryClient("");
    // GetAccessControlList will fail on storage accounts
    // without hierarchical namespace enabled.
    directory_client.GetAccessControlList();
    return HNSSupport::kEnabled;
  } catch (std::out_of_range& exception) {
    // Azurite issue detected.
    DCHECK(IsDfsEmulator(options));
    return HNSSupport::kDisabled;
  } catch (const Storage::StorageException& exception) {
    // Flat namespace storage accounts with "soft delete" enabled return
    //
    //   "Conflict - This endpoint does not support BlobStorageEvents
    //   or SoftDelete. [...]" [1],
    //
    // otherwise it returns:
    //
    //   "BadRequest - This operation is only supported on a hierarchical namespace
    //   account."
    //
    // [1]:
    // https://learn.microsoft.com/en-us/answers/questions/1069779/this-endpoint-does-not-support-blobstorageevents-o
    switch (exception.StatusCode) {
      case Http::HttpStatusCode::BadRequest:
      case Http::HttpStatusCode::Conflict:
        return HNSSupport::kDisabled;
      case Http::HttpStatusCode::NotFound:
        if (IsDfsEmulator(options)) {
          return HNSSupport::kDisabled;
        }
        // Did we get an error because of the container not existing?
        if (IsContainerNotFound(exception)) {
          return HNSSupport::kContainerNotFound;
        }
        [[fallthrough]];
      default:
        if (exception.ErrorCode == "HierarchicalNamespaceNotEnabled") {
          return HNSSupport::kDisabled;
        }
        return ExceptionToStatus(exception,
                                 "Check for Hierarchical Namespace support on '",
                                 adlfs_client.GetUrl(), "' failed.");
    }
  }
}

}  // namespace internal

// -----------------------------------------------------------------------
// AzureFilesystem Implementation

namespace {

// In Azure Storage terminology, a "container" and a "filesystem" are the same
// kind of object, but it can be accessed using different APIs. The Blob Storage
// API calls it a "container", the Data Lake Storage Gen 2 API calls it a
// "filesystem". Creating a container using the Blob Storage API will make it
// accessible using the Data Lake Storage Gen 2 API and vice versa.

const char kDelimiter[] = {internal::kSep, '\0'};

/// \pre location.container is not empty.
template <class ContainerClient>
Result<FileInfo> GetContainerPropsAsFileInfo(const AzureLocation& location,
                                             const ContainerClient& container_client) {
  DCHECK(!location.container.empty());
  FileInfo info{location.path.empty() ? location.all : location.container};
  try {
    auto properties = container_client.GetProperties();
    info.set_type(FileType::Directory);
    info.set_mtime(std::chrono::system_clock::time_point{properties.Value.LastModified});
    return info;
  } catch (const Storage::StorageException& exception) {
    if (IsContainerNotFound(exception)) {
      info.set_type(FileType::NotFound);
      return info;
    }
    return ExceptionToStatus(exception, "GetProperties for '", container_client.GetUrl(),
                             "' failed.");
  }
}

template <class ContainerClient>
Status CreateContainerIfNotExists(const std::string& container_name,
                                  const ContainerClient& container_client) {
  try {
    container_client.CreateIfNotExists();
    return Status::OK();
  } catch (const Storage::StorageException& exception) {
    return ExceptionToStatus(exception, "Failed to create a container: ", container_name,
                             ": ", container_client.GetUrl());
  }
}

FileInfo DirectoryFileInfoFromPath(std::string_view path) {
  return FileInfo{std::string{internal::RemoveTrailingSlash(path)}, FileType::Directory};
}

FileInfo FileInfoFromBlob(std::string_view container,
                          const Blobs::Models::BlobItem& blob) {
  auto path = internal::ConcatAbstractPath(container, blob.Name);
  if (internal::HasTrailingSlash(blob.Name)) {
    return DirectoryFileInfoFromPath(path);
  }
  FileInfo info{std::move(path), FileType::File};
  info.set_size(blob.BlobSize);
  info.set_mtime(std::chrono::system_clock::time_point{blob.Details.LastModified});
  return info;
}

}  // namespace

class AzureFileSystem::Impl {
 private:
  io::IOContext io_context_;
  AzureOptions options_;

  std::unique_ptr<DataLake::DataLakeServiceClient> datalake_service_client_;
  std::unique_ptr<Blobs::BlobServiceClient> blob_service_client_;
  HNSSupport cached_hns_support_ = HNSSupport::kUnknown;

  Impl(AzureOptions options, io::IOContext io_context)
      : io_context_(std::move(io_context)), options_(std::move(options)) {}

 public:
  static Result<std::unique_ptr<AzureFileSystem::Impl>> Make(AzureOptions options,
                                                             io::IOContext io_context) {
    auto self = std::unique_ptr<AzureFileSystem::Impl>(
        new AzureFileSystem::Impl(std::move(options), std::move(io_context)));
    ARROW_ASSIGN_OR_RAISE(self->blob_service_client_,
                          self->options_.MakeBlobServiceClient());
    ARROW_ASSIGN_OR_RAISE(self->datalake_service_client_,
                          self->options_.MakeDataLakeServiceClient());
    return self;
  }

  io::IOContext& io_context() { return io_context_; }
  const AzureOptions& options() const { return options_; }

  Blobs::BlobContainerClient GetBlobContainerClient(const std::string& container_name) {
    return blob_service_client_->GetBlobContainerClient(container_name);
  }

  /// \param container_name Also known as "filesystem" in the ADLS Gen2 API.
  DataLake::DataLakeFileSystemClient GetFileSystemClient(
      const std::string& container_name) {
    return datalake_service_client_->GetFileSystemClient(container_name);
  }

  /// \brief Memoized version of CheckIfHierarchicalNamespaceIsEnabled.
  ///
  /// \return kEnabled/kDisabled/kContainerNotFound (kUnknown is never returned).
  Result<HNSSupport> HierarchicalNamespaceSupport(
      const DataLake::DataLakeFileSystemClient& adlfs_client) {
    switch (cached_hns_support_) {
      case HNSSupport::kEnabled:
      case HNSSupport::kDisabled:
        return cached_hns_support_;
      case HNSSupport::kUnknown:
      case HNSSupport::kContainerNotFound:
        // Try the check again because the support is still unknown or the container
        // that didn't exist before may exist now.
        break;
    }
    ARROW_ASSIGN_OR_RAISE(
        auto hns_support,
        internal::CheckIfHierarchicalNamespaceIsEnabled(adlfs_client, options_));
    DCHECK_NE(hns_support, HNSSupport::kUnknown);
    if (hns_support == HNSSupport::kContainerNotFound) {
      // Caller should handle kContainerNotFound case appropriately as it knows the
      // container this refers to, but the cached value in that case should remain
      // kUnknown before we get a CheckIfHierarchicalNamespaceIsEnabled result that
      // is not kContainerNotFound.
      cached_hns_support_ = HNSSupport::kUnknown;
    } else {
      cached_hns_support_ = hns_support;
    }
    return hns_support;
  }

  /// This is used from unit tests to ensure we perform operations on all the
  /// possible states of cached_hns_support_.
  void ForceCachedHierarchicalNamespaceSupport(int support) {
    auto hns_support = static_cast<HNSSupport>(support);
    switch (hns_support) {
      case HNSSupport::kUnknown:
      case HNSSupport::kContainerNotFound:
      case HNSSupport::kDisabled:
      case HNSSupport::kEnabled:
        cached_hns_support_ = hns_support;
        return;
    }
    // This is reachable if an invalid int is cast to enum class HNSSupport.
    DCHECK(false) << "Invalid enum HierarchicalNamespaceSupport value.";
  }

  /// \pre location.path is not empty.
  Result<FileInfo> GetFileInfo(const DataLake::DataLakeFileSystemClient& adlfs_client,
                               const AzureLocation& location) {
    auto file_client = adlfs_client.GetFileClient(location.path);
    try {
      FileInfo info{location.all};
      auto properties = file_client.GetProperties();
      if (properties.Value.IsDirectory) {
        info.set_type(FileType::Directory);
      } else if (internal::HasTrailingSlash(location.path)) {
        // For a path with a trailing slash, a Hierarchical Namespace storage account
        // may recognize a file (path with trailing slash removed). For consistency
        // with other arrow::FileSystem implementations we chose to return NotFound
        // because the trailing slash means the user was looking for a directory.
        info.set_type(FileType::NotFound);
        return info;
      } else {
        info.set_type(FileType::File);
        info.set_size(properties.Value.FileSize);
      }
      info.set_mtime(
          std::chrono::system_clock::time_point{properties.Value.LastModified});
      return info;
    } catch (const Storage::StorageException& exception) {
      if (exception.StatusCode == Http::HttpStatusCode::NotFound) {
        return FileInfo{location.all, FileType::NotFound};
      }
      return ExceptionToStatus(
          exception, "GetProperties for '", file_client.GetUrl(),
          "' failed. GetFileInfo is unable to determine whether the path exists.");
    }
  }

  /// On flat namespace accounts there are no real directories. Directories are
  /// implied by empty directory marker blobs with names ending in "/" or there
  /// being blobs with names starting with the directory path.
  ///
  /// \pre location.path is not empty.
  Result<FileInfo> GetFileInfo(const Blobs::BlobContainerClient& container_client,
                               const AzureLocation& location) {
    DCHECK(!location.path.empty());
    Blobs::ListBlobsOptions options;
    options.Prefix = internal::RemoveTrailingSlash(location.path);
    options.PageSizeHint = 1;

    try {
      FileInfo info{location.all};
      auto list_response = container_client.ListBlobsByHierarchy(kDelimiter, options);
      // Since PageSizeHint=1, we expect at most one entry in either Blobs or
      // BlobPrefixes. A BlobPrefix always ends with kDelimiter ("/"), so we can
      // distinguish between a directory and a file by checking if we received a
      // prefix or a blob.
      if (!list_response.BlobPrefixes.empty()) {
        // Ensure the returned BlobPrefixes[0] string doesn't contain more characters than
        // the requested Prefix. For instance, if we request with Prefix="dir/abra" and
        // the container contains "dir/abracadabra/" but not "dir/abra/", we will get back
        // "dir/abracadabra/" in the BlobPrefixes list. If "dir/abra/" existed,
        // it would be returned instead because it comes before "dir/abracadabra/" in the
        // lexicographic order guaranteed by ListBlobsByHierarchy.
        const auto& blob_prefix = list_response.BlobPrefixes[0];
        if (blob_prefix == internal::EnsureTrailingSlash(location.path)) {
          info.set_type(FileType::Directory);
          return info;
        }
      }
      if (!list_response.Blobs.empty()) {
        const auto& blob = list_response.Blobs[0];
        if (blob.Name == location.path) {
          info.set_type(FileType::File);
          info.set_size(blob.BlobSize);
          info.set_mtime(
              std::chrono::system_clock::time_point{blob.Details.LastModified});
          return info;
        }
      }
      info.set_type(FileType::NotFound);
      return info;
    } catch (const Storage::StorageException& exception) {
      if (IsContainerNotFound(exception)) {
        return FileInfo{location.all, FileType::NotFound};
      }
      return ExceptionToStatus(
          exception, "ListBlobsByHierarchy failed for prefix='", *options.Prefix,
          "'. GetFileInfo is unable to determine whether the path exists.");
    }
  }

 private:
  /// \pref location.container is not empty.
  template <typename ContainerClient>
  Status CheckDirExists(const ContainerClient& container_client,
                        const AzureLocation& location) {
    DCHECK(!location.container.empty());
    FileInfo info;
    if (location.path.empty()) {
      ARROW_ASSIGN_OR_RAISE(info,
                            GetContainerPropsAsFileInfo(location, container_client));
    } else {
      ARROW_ASSIGN_OR_RAISE(info, GetFileInfo(container_client, location));
    }
    if (info.type() == FileType::NotFound) {
      return PathNotFound(location);
    }
    DCHECK_EQ(info.type(), FileType::Directory);
    return Status::OK();
  }

  template <typename OnContainer>
  Status VisitContainers(const Core::Context& context, OnContainer&& on_container) const {
    Blobs::ListBlobContainersOptions options;
    try {
      auto container_list_response =
          blob_service_client_->ListBlobContainers(options, context);
      for (; container_list_response.HasPage();
           container_list_response.MoveToNextPage(context)) {
        for (const auto& container : container_list_response.BlobContainers) {
          RETURN_NOT_OK(on_container(container));
        }
      }
    } catch (const Storage::StorageException& exception) {
      return ExceptionToStatus(exception, "Failed to list account containers.");
    }
    return Status::OK();
  }

  static std::string_view BasenameView(std::string_view s) {
    DCHECK(!internal::HasTrailingSlash(s));
    auto offset = s.find_last_of(internal::kSep);
    auto result = (offset == std::string_view::npos) ? s : s.substr(offset);
    DCHECK(!result.empty() && result.back() != internal::kSep);
    return result;
  }

  /// \brief List the blobs at the root of a container or some dir in a container.
  ///
  /// \pre container_client is the client for the container named like the first
  /// segment of select.base_dir.
  Status GetFileInfoWithSelectorFromContainer(
      const Blobs::BlobContainerClient& container_client, const Core::Context& context,
      Azure::Nullable<int32_t> page_size_hint, const FileSelector& select,
      FileInfoVector* acc_results) {
    ARROW_ASSIGN_OR_RAISE(auto base_location, AzureLocation::FromString(select.base_dir));

    bool found = false;
    Blobs::ListBlobsOptions options;
    if (internal::IsEmptyPath(base_location.path)) {
      // If the base_dir is the root of the container, then we want to list all blobs in
      // the container and the Prefix should be empty and not even include the trailing
      // slash because the container itself represents the `<container>/` directory.
      options.Prefix = {};
      found = true;  // Unless the container itself is not found later!
    } else {
      options.Prefix = internal::EnsureTrailingSlash(base_location.path);
    }
    options.PageSizeHint = page_size_hint;
    options.Include = Blobs::Models::ListBlobsIncludeFlags::Metadata;

    auto recurse = [&](const std::string& blob_prefix) noexcept -> Status {
      if (select.recursive && select.max_recursion > 0) {
        FileSelector sub_select;
        sub_select.base_dir = internal::ConcatAbstractPath(
            base_location.container, internal::RemoveTrailingSlash(blob_prefix));
        sub_select.allow_not_found = true;
        sub_select.recursive = true;
        sub_select.max_recursion = select.max_recursion - 1;
        return GetFileInfoWithSelectorFromContainer(
            container_client, context, page_size_hint, sub_select, acc_results);
      }
      return Status::OK();
    };

    auto process_blob = [&](const Blobs::Models::BlobItem& blob) noexcept {
      // blob.Name has trailing slash only when Prefix is an empty
      // directory marker blob for the directory we're listing
      // from, and we should skip it.
      if (!internal::HasTrailingSlash(blob.Name)) {
        acc_results->push_back(FileInfoFromBlob(base_location.container, blob));
      }
    };
    auto process_prefix = [&](const std::string& prefix) noexcept -> Status {
      const auto path = internal::ConcatAbstractPath(base_location.container, prefix);
      acc_results->push_back(DirectoryFileInfoFromPath(path));
      return recurse(prefix);
    };

    try {
      auto list_response =
          container_client.ListBlobsByHierarchy(/*delimiter=*/"/", options, context);
      for (; list_response.HasPage(); list_response.MoveToNextPage(context)) {
        if (list_response.Blobs.empty() && list_response.BlobPrefixes.empty()) {
          continue;
        }
        found = true;
        // Blob and BlobPrefixes are sorted by name, so we can merge-iterate
        // them to ensure returned results are all sorted.
        size_t blob_index = 0;
        size_t blob_prefix_index = 0;
        while (blob_index < list_response.Blobs.size() &&
               blob_prefix_index < list_response.BlobPrefixes.size()) {
          const auto& blob = list_response.Blobs[blob_index];
          const auto& prefix = list_response.BlobPrefixes[blob_prefix_index];
          const int cmp = blob.Name.compare(prefix);
          if (cmp < 0) {
            process_blob(blob);
            blob_index += 1;
          } else if (cmp > 0) {
            RETURN_NOT_OK(process_prefix(prefix));
            blob_prefix_index += 1;
          } else {
            DCHECK_EQ(blob.Name, prefix);
            RETURN_NOT_OK(process_prefix(prefix));
            blob_index += 1;
            blob_prefix_index += 1;
            // If the container has an empty dir marker blob and another blob starting
            // with this blob name as a prefix, the blob doesn't appear in the listing
            // that also contains the prefix, so AFAICT this branch in unreachable. The
            // code above is kept just in case, but if this DCHECK(false) is ever reached,
            // we should refactor this loop to ensure no duplicate entries are ever
            // reported.
            DCHECK(false)
                << "Unexpected blob/prefix name collision on the same listing request";
          }
        }
        for (; blob_index < list_response.Blobs.size(); blob_index++) {
          process_blob(list_response.Blobs[blob_index]);
        }
        for (; blob_prefix_index < list_response.BlobPrefixes.size();
             blob_prefix_index++) {
          RETURN_NOT_OK(process_prefix(list_response.BlobPrefixes[blob_prefix_index]));
        }
      }
    } catch (const Storage::StorageException& exception) {
      if (IsContainerNotFound(exception)) {
        found = false;
      } else {
        return ExceptionToStatus(exception,
                                 "Failed to list blobs in a directory: ", select.base_dir,
                                 ": ", container_client.GetUrl());
      }
    }

    return found || select.allow_not_found
               ? Status::OK()
               : ::arrow::fs::internal::PathNotFound(select.base_dir);
  }

 public:
  Status GetFileInfoWithSelector(const Core::Context& context,
                                 Azure::Nullable<int32_t> page_size_hint,
                                 const FileSelector& select,
                                 FileInfoVector* acc_results) {
    ARROW_ASSIGN_OR_RAISE(auto base_location, AzureLocation::FromString(select.base_dir));

    if (base_location.container.empty()) {
      // Without a container, the base_location is equivalent to the filesystem
      // root -- `/`. FileSelector::allow_not_found doesn't matter in this case
      // because the root always exists.
      auto on_container = [&](const Blobs::Models::BlobContainerItem& container) {
        // Deleted containers are not listed by ListContainers.
        DCHECK(!container.IsDeleted);

        // Every container is considered a directory.
        FileInfo info{container.Name, FileType::Directory};
        info.set_mtime(
            std::chrono::system_clock::time_point{container.Details.LastModified});
        acc_results->push_back(std::move(info));

        // Recurse into containers (subdirectories) if requested.
        if (select.recursive && select.max_recursion > 0) {
          FileSelector sub_select;
          sub_select.base_dir = container.Name;
          sub_select.allow_not_found = true;
          sub_select.recursive = true;
          sub_select.max_recursion = select.max_recursion - 1;
          ARROW_RETURN_NOT_OK(
              GetFileInfoWithSelector(context, page_size_hint, sub_select, acc_results));
        }
        return Status::OK();
      };
      return VisitContainers(context, std::move(on_container));
    }

    auto container_client =
        blob_service_client_->GetBlobContainerClient(base_location.container);
    return GetFileInfoWithSelectorFromContainer(container_client, context, page_size_hint,
                                                select, acc_results);
  }

  Result<std::shared_ptr<ObjectInputFile>> OpenInputFile(const AzureLocation& location,
                                                         AzureFileSystem* fs) {
    RETURN_NOT_OK(ValidateFileLocation(location));
    auto blob_client = std::make_shared<Blobs::BlobClient>(
        blob_service_client_->GetBlobContainerClient(location.container)
            .GetBlobClient(location.path));

    auto ptr = std::make_shared<ObjectInputFile>(blob_client, fs->io_context(),
                                                 std::move(location));
    RETURN_NOT_OK(ptr->Init());
    return ptr;
  }

  Result<std::shared_ptr<ObjectInputFile>> OpenInputFile(const FileInfo& info,
                                                         AzureFileSystem* fs) {
    if (info.type() == FileType::NotFound) {
      return ::arrow::fs::internal::PathNotFound(info.path());
    }
    if (info.type() != FileType::File && info.type() != FileType::Unknown) {
      return ::arrow::fs::internal::NotAFile(info.path());
    }
    ARROW_ASSIGN_OR_RAISE(auto location, AzureLocation::FromString(info.path()));
    RETURN_NOT_OK(ValidateFileLocation(location));
    auto blob_client = std::make_shared<Blobs::BlobClient>(
        blob_service_client_->GetBlobContainerClient(location.container)
            .GetBlobClient(location.path));

    auto ptr = std::make_shared<ObjectInputFile>(blob_client, fs->io_context(),
                                                 std::move(location), info.size());
    RETURN_NOT_OK(ptr->Init());
    return ptr;
  }

 private:
  /// This function cannot assume the filesystem/container already exists.
  ///
  /// \pre location.container is not empty.
  /// \pre location.path is not empty.
  template <class ContainerClient, class CreateDirIfNotExists>
  Status CreateDirTemplate(const ContainerClient& container_client,
                           CreateDirIfNotExists&& create_if_not_exists,
                           const AzureLocation& location, bool recursive) {
    DCHECK(!location.container.empty());
    DCHECK(!location.path.empty());
    // Non-recursive CreateDir calls require the parent directory to exist.
    if (!recursive) {
      auto parent = location.parent();
      if (!parent.path.empty()) {
        RETURN_NOT_OK(CheckDirExists(container_client, parent));
      }
      // If the parent location is just the container, we don't need to check if it
      // exists because the operation we perform below will fail if the container
      // doesn't exist and we can handle that error according to the recursive flag.
    }
    try {
      create_if_not_exists(container_client, location);
      return Status::OK();
    } catch (const Storage::StorageException& exception) {
      if (IsContainerNotFound(exception)) {
        try {
          if (recursive) {
            container_client.CreateIfNotExists();
            create_if_not_exists(container_client, location);
            return Status::OK();
          } else {
            auto parent = location.parent();
            return PathNotFound(parent);
          }
        } catch (const Storage::StorageException& second_exception) {
          return ExceptionToStatus(second_exception, "Failed to create directory '",
                                   location.all, "': ", container_client.GetUrl());
        }
      }
      return ExceptionToStatus(exception, "Failed to create directory '", location.all,
                               "': ", container_client.GetUrl());
    }
  }

 public:
  /// This function cannot assume the filesystem already exists.
  ///
  /// \pre location.container is not empty.
  /// \pre location.path is not empty.
  Status CreateDirOnFileSystem(const DataLake::DataLakeFileSystemClient& adlfs_client,
                               const AzureLocation& location, bool recursive) {
    return CreateDirTemplate(
        adlfs_client,
        [](const auto& adlfs_client, const auto& location) {
          auto directory_client = adlfs_client.GetDirectoryClient(location.path);
          directory_client.CreateIfNotExists();
        },
        location, recursive);
  }

  /// This function cannot assume the container already exists.
  ///
  /// \pre location.container is not empty.
  /// \pre location.path is not empty.
  Status CreateDirOnContainer(const Blobs::BlobContainerClient& container_client,
                              const AzureLocation& location, bool recursive) {
    return CreateDirTemplate(
        container_client,
        [this](const auto& container_client, const auto& location) {
          EnsureEmptyDirExistsImplThatThrows(container_client, location.path);
        },
        location, recursive);
  }

  Result<std::shared_ptr<ObjectAppendStream>> OpenAppendStream(
      const AzureLocation& location,
      const std::shared_ptr<const KeyValueMetadata>& metadata, const bool truncate,
      AzureFileSystem* fs) {
    RETURN_NOT_OK(ValidateFileLocation(location));

    auto block_blob_client = std::make_shared<Blobs::BlockBlobClient>(
        blob_service_client_->GetBlobContainerClient(location.container)
            .GetBlockBlobClient(location.path));

    std::shared_ptr<ObjectAppendStream> stream;
    if (truncate) {
      RETURN_NOT_OK(CreateEmptyBlockBlob(*block_blob_client));
      stream = std::make_shared<ObjectAppendStream>(block_blob_client, fs->io_context(),
                                                    location, metadata, options_, 0);
    } else {
      stream = std::make_shared<ObjectAppendStream>(block_blob_client, fs->io_context(),
                                                    location, metadata, options_);
    }
    RETURN_NOT_OK(stream->Init());
    return stream;
  }

 private:
  void EnsureEmptyDirExistsImplThatThrows(
      const Blobs::BlobContainerClient& container_client,
      const std::string& path_within_container) {
    auto dir_marker_blob_path = internal::EnsureTrailingSlash(path_within_container);
    auto block_blob_client =
        container_client.GetBlobClient(dir_marker_blob_path).AsBlockBlobClient();
    // Attach metadata that other filesystem implementations expect to be present
    // on directory marker blobs.
    // https://github.com/fsspec/adlfs/blob/32132c4094350fca2680155a5c236f2e9f991ba5/adlfs/spec.py#L855-L870
    Blobs::UploadBlockBlobFromOptions blob_options;
    blob_options.Metadata.emplace("is_directory", "true");
    block_blob_client.UploadFrom(nullptr, 0, blob_options);
  }

 public:
  /// This function assumes the container already exists. So it can only be
  /// called after that has been verified.
  ///
  /// \pre location.container is not empty.
  /// \pre The location.container container already exists.
  Status EnsureEmptyDirExists(const Blobs::BlobContainerClient& container_client,
                              const AzureLocation& location, const char* operation_name) {
    DCHECK(!location.container.empty());
    if (location.path.empty()) {
      // Nothing to do. The container already exists per the preconditions.
      return Status::OK();
    }
    try {
      EnsureEmptyDirExistsImplThatThrows(container_client, location.path);
      return Status::OK();
    } catch (const Storage::StorageException& exception) {
      return ExceptionToStatus(
          exception, operation_name, " failed to ensure empty directory marker '",
          location.path, "' exists in container: ", container_client.GetUrl());
    }
  }

  /// \pre location.container is not empty.
  /// \pre location.path is empty.
  Status DeleteContainer(const Blobs::BlobContainerClient& container_client,
                         const AzureLocation& location) {
    DCHECK(!location.container.empty());
    DCHECK(location.path.empty());
    try {
      auto response = container_client.Delete();
      if (response.Value.Deleted) {
        return Status::OK();
      } else {
        return StatusFromErrorResponse(
            container_client.GetUrl(), *response.RawResponse,
            "Failed to delete a container: " + location.container);
      }
    } catch (const Storage::StorageException& exception) {
      if (IsContainerNotFound(exception)) {
        return PathNotFound(location);
      }
      return ExceptionToStatus(exception,
                               "Failed to delete a container: ", location.container, ": ",
                               container_client.GetUrl());
    }
  }

  /// Deletes contents of a directory and possibly the directory itself
  /// depending on the value of preserve_dir_marker_blob.
  ///
  /// \pre location.container is not empty.
  /// \pre preserve_dir_marker_blob=false implies location.path is not empty
  /// because we can't *not preserve* the root directory of a container.
  ///
  /// \param require_dir_to_exist Require the directory to exist *before* this
  /// operation, otherwise return PathNotFound.
  /// \param preserve_dir_marker_blob Ensure the empty directory marker blob
  /// is preserved (not deleted) or created (before the contents are deleted) if it
  /// doesn't exist explicitly but is implied by the existence of blobs with names
  /// starting with the directory path.
  /// \param operation_name Used in error messages to accurately describe the operation
  Status DeleteDirContentsOnContainer(const Blobs::BlobContainerClient& container_client,
                                      const AzureLocation& location,
                                      bool require_dir_to_exist,
                                      bool preserve_dir_marker_blob,
                                      const char* operation_name) {
    using DeleteBlobResponse = Storage::DeferredResponse<Blobs::Models::DeleteBlobResult>;
    DCHECK(!location.container.empty());
    DCHECK(preserve_dir_marker_blob || !location.path.empty())
        << "Must pass preserve_dir_marker_blob=true when location.path is empty "
           "(i.e. deleting the contents of a container).";
    Blobs::ListBlobsOptions options;
    if (!location.path.empty()) {
      options.Prefix = internal::EnsureTrailingSlash(location.path);
    }
    // https://learn.microsoft.com/en-us/rest/api/storageservices/blob-batch#remarks
    //
    // Only supports up to 256 subrequests in a single batch. The
    // size of the body for a batch request can't exceed 4 MB.
    const int32_t kNumMaxRequestsInBatch = 256;
    options.PageSizeHint = kNumMaxRequestsInBatch;
    // trusted only if preserve_dir_marker_blob is true.
    bool found_dir_marker_blob = false;
    try {
      auto list_response = container_client.ListBlobs(options);
      if (require_dir_to_exist && list_response.Blobs.empty()) {
        return PathNotFound(location);
      }
      for (; list_response.HasPage(); list_response.MoveToNextPage()) {
        if (list_response.Blobs.empty()) {
          continue;
        }
        auto batch = container_client.CreateBatch();
        std::vector<std::pair<std::string_view, DeleteBlobResponse>> deferred_responses;
        for (const auto& blob_item : list_response.Blobs) {
          if (preserve_dir_marker_blob && !found_dir_marker_blob) {
            const bool is_dir_marker_blob =
                options.Prefix.HasValue() && blob_item.Name == *options.Prefix;
            if (is_dir_marker_blob) {
              // Skip deletion of the existing directory marker blob,
              // but take note that it exists.
              found_dir_marker_blob = true;
              continue;
            }
          }
          deferred_responses.emplace_back(blob_item.Name,
                                          batch.DeleteBlob(blob_item.Name));
        }
        try {
          // Before submitting the batch deleting directory contents, ensure
          // the empty directory marker blob exists. Doing this first, means that
          // directory doesn't "stop existing" during the duration of the batch delete
          // operation.
          if (preserve_dir_marker_blob && !found_dir_marker_blob) {
            // Only create an empty directory marker blob if the directory's
            // existence is implied by the existence of blobs with names
            // starting with the directory path.
            if (!deferred_responses.empty()) {
              RETURN_NOT_OK(
                  EnsureEmptyDirExists(container_client, location, operation_name));
            }
          }
          if (!deferred_responses.empty()) {
            container_client.SubmitBatch(batch);
          }
        } catch (const Storage::StorageException& exception) {
          return ExceptionToStatus(exception, "Failed to delete blobs in a directory: ",
                                   location.path, ": ", container_client.GetUrl());
        }
        std::vector<std::string> failed_blob_names;
        for (auto& [blob_name_view, deferred_response] : deferred_responses) {
          bool success = true;
          try {
            auto delete_result = deferred_response.GetResponse();
            success = delete_result.Value.Deleted;
          } catch (const Storage::StorageException& exception) {
            success = false;
          }
          if (!success) {
            failed_blob_names.emplace_back(blob_name_view);
          }
        }
        if (!failed_blob_names.empty()) {
          if (failed_blob_names.size() == 1) {
            return Status::IOError("Failed to delete a blob: ", failed_blob_names[0],
                                   ": " + container_client.GetUrl());
          } else {
            return Status::IOError("Failed to delete blobs: [",
                                   arrow::internal::JoinStrings(failed_blob_names, ", "),
                                   "]: " + container_client.GetUrl());
          }
        }
      }
      return Status::OK();
    } catch (const Storage::StorageException& exception) {
      return ExceptionToStatus(exception,
                               "Failed to list blobs in a directory: ", location.path,
                               ": ", container_client.GetUrl());
    }
  }

  /// \pre location.container is not empty.
  /// \pre location.path is not empty.
  Status DeleteDirOnFileSystem(const DataLake::DataLakeFileSystemClient& adlfs_client,
                               const AzureLocation& location) {
    DCHECK(!location.container.empty());
    DCHECK(!location.path.empty());
    auto directory_client = adlfs_client.GetDirectoryClient(location.path);
    // XXX: should "directory not found" be considered an error?
    try {
      auto response = directory_client.DeleteRecursive();
      if (response.Value.Deleted) {
        return Status::OK();
      } else {
        return StatusFromErrorResponse(directory_client.GetUrl(), *response.RawResponse,
                                       "Failed to delete a directory: " + location.path);
      }
    } catch (const Storage::StorageException& exception) {
      return ExceptionToStatus(exception, "Failed to delete a directory: ", location.path,
                               ": ", directory_client.GetUrl());
    }
  }

  /// \pre location.container is not empty.
  Status DeleteDirContentsOnFileSystem(
      const DataLake::DataLakeFileSystemClient& adlfs_client,
      const AzureLocation& location, bool missing_dir_ok) {
    auto directory_client = adlfs_client.GetDirectoryClient(location.path);
    try {
      auto list_response = directory_client.ListPaths(false);
      for (; list_response.HasPage(); list_response.MoveToNextPage()) {
        for (const auto& path : list_response.Paths) {
          if (path.IsDirectory) {
            auto sub_directory_client = adlfs_client.GetDirectoryClient(path.Name);
            try {
              sub_directory_client.DeleteRecursive();
            } catch (const Storage::StorageException& exception) {
              return ExceptionToStatus(
                  exception, "Failed to delete a sub directory: ", location.container,
                  kDelimiter, path.Name, ": ", sub_directory_client.GetUrl());
            }
          } else {
            auto sub_file_client = adlfs_client.GetFileClient(path.Name);
            try {
              sub_file_client.Delete();
            } catch (const Storage::StorageException& exception) {
              return ExceptionToStatus(
                  exception, "Failed to delete a sub file: ", location.container,
                  kDelimiter, path.Name, ": ", sub_file_client.GetUrl());
            }
          }
        }
      }
      return Status::OK();
    } catch (const Storage::StorageException& exception) {
      if (missing_dir_ok && exception.StatusCode == Http::HttpStatusCode::NotFound) {
        return Status::OK();
      }
      return ExceptionToStatus(exception,
                               "Failed to delete directory contents: ", location.path,
                               ": ", directory_client.GetUrl());
    }
  }

  Status CopyFile(const AzureLocation& src, const AzureLocation& dest) {
    RETURN_NOT_OK(ValidateFileLocation(src));
    RETURN_NOT_OK(ValidateFileLocation(dest));
    if (src == dest) {
      return Status::OK();
    }
    auto dest_blob_client = blob_service_client_->GetBlobContainerClient(dest.container)
                                .GetBlobClient(dest.path);
    auto src_url = blob_service_client_->GetBlobContainerClient(src.container)
                       .GetBlobClient(src.path)
                       .GetUrl();
    try {
      dest_blob_client.CopyFromUri(src_url);
    } catch (const Storage::StorageException& exception) {
      return ExceptionToStatus(exception, "Failed to copy a blob. (", src_url, " -> ",
                               dest_blob_client.GetUrl(), ")");
    }
    return Status::OK();
  }
};

AzureFileSystem::AzureFileSystem(std::unique_ptr<Impl>&& impl)
    : FileSystem(impl->io_context()), impl_(std::move(impl)) {
  default_async_is_sync_ = false;
}

void AzureFileSystem::ForceCachedHierarchicalNamespaceSupport(int hns_support) {
  impl_->ForceCachedHierarchicalNamespaceSupport(hns_support);
}

Result<std::shared_ptr<AzureFileSystem>> AzureFileSystem::Make(
    const AzureOptions& options, const io::IOContext& io_context) {
  ARROW_ASSIGN_OR_RAISE(auto impl, AzureFileSystem::Impl::Make(options, io_context));
  return std::shared_ptr<AzureFileSystem>(new AzureFileSystem(std::move(impl)));
}

const AzureOptions& AzureFileSystem::options() const { return impl_->options(); }

bool AzureFileSystem::Equals(const FileSystem& other) const {
  if (this == &other) {
    return true;
  }
  if (other.type_name() != type_name()) {
    return false;
  }
  const auto& azure_fs = ::arrow::internal::checked_cast<const AzureFileSystem&>(other);
  return options().Equals(azure_fs.options());
}

Result<FileInfo> AzureFileSystem::GetFileInfo(const std::string& path) {
  ARROW_ASSIGN_OR_RAISE(auto location, AzureLocation::FromString(path));
  if (location.container.empty()) {
    DCHECK(location.path.empty());
    // Root directory of the storage account.
    return FileInfo{"", FileType::Directory};
  }
  if (location.path.empty()) {
    // We have a container, but no path within the container.
    // The container itself represents a directory.
    auto container_client = impl_->GetBlobContainerClient(location.container);
    return GetContainerPropsAsFileInfo(location, container_client);
  }
  // There is a path to search within the container. Check HNS support to proceed.
  auto adlfs_client = impl_->GetFileSystemClient(location.container);
  ARROW_ASSIGN_OR_RAISE(auto hns_support,
                        impl_->HierarchicalNamespaceSupport(adlfs_client));
  if (hns_support == HNSSupport::kContainerNotFound) {
    return FileInfo{location.all, FileType::NotFound};
  }
  if (hns_support == HNSSupport::kEnabled) {
    return impl_->GetFileInfo(adlfs_client, location);
  }
  DCHECK_EQ(hns_support, HNSSupport::kDisabled);
  auto container_client = impl_->GetBlobContainerClient(location.container);
  return impl_->GetFileInfo(container_client, location);
}

Result<FileInfoVector> AzureFileSystem::GetFileInfo(const FileSelector& select) {
  Core::Context context;
  Azure::Nullable<int32_t> page_size_hint;  // unspecified
  FileInfoVector results;
  RETURN_NOT_OK(
      impl_->GetFileInfoWithSelector(context, page_size_hint, select, &results));
  return {std::move(results)};
}

Status AzureFileSystem::CreateDir(const std::string& path, bool recursive) {
  ARROW_ASSIGN_OR_RAISE(auto location, AzureLocation::FromString(path));
  if (location.container.empty()) {
    return Status::Invalid("CreateDir requires a non-empty path.");
  }

  auto container_client = impl_->GetBlobContainerClient(location.container);
  if (location.path.empty()) {
    // If the path is just the container, the parent (root) trivially exists,
    // and the CreateDir operation comes down to just creating the container.
    return CreateContainerIfNotExists(location.container, container_client);
  }

  auto adlfs_client = impl_->GetFileSystemClient(location.container);
  ARROW_ASSIGN_OR_RAISE(auto hns_support,
                        impl_->HierarchicalNamespaceSupport(adlfs_client));
  if (hns_support == HNSSupport::kContainerNotFound) {
    if (!recursive) {
      auto parent = location.parent();
      return PathNotFound(parent);
    }
    RETURN_NOT_OK(CreateContainerIfNotExists(location.container, container_client));
    // Perform a second check for HNS support after creating the container.
    ARROW_ASSIGN_OR_RAISE(hns_support, impl_->HierarchicalNamespaceSupport(adlfs_client));
    if (hns_support == HNSSupport::kContainerNotFound) {
      // We only get kContainerNotFound if we are unable to read the properties of the
      // container we just created. This is very unlikely, but theoretically possible in
      // a concurrent system, so the error is handled to avoid infinite recursion.
      return Status::IOError("Unable to read properties of a newly created container: ",
                             location.container, ": " + container_client.GetUrl());
    }
  }
  // CreateDirOnFileSystem and CreateDirOnContainer can handle the container
  // not existing which is useful and necessary here since the only reason
  // a container was created above was to check for HNS support when it wasn't
  // cached yet.
  if (hns_support == HNSSupport::kEnabled) {
    return impl_->CreateDirOnFileSystem(adlfs_client, location, recursive);
  }
  DCHECK_EQ(hns_support, HNSSupport::kDisabled);
  return impl_->CreateDirOnContainer(container_client, location, recursive);
}

Status AzureFileSystem::DeleteDir(const std::string& path) {
  ARROW_ASSIGN_OR_RAISE(auto location, AzureLocation::FromString(path));
  if (location.container.empty()) {
    return Status::Invalid("DeleteDir requires a non-empty path.");
  }
  if (location.path.empty()) {
    auto container_client = impl_->GetBlobContainerClient(location.container);
    return impl_->DeleteContainer(container_client, location);
  }

  auto adlfs_client = impl_->GetFileSystemClient(location.container);
  ARROW_ASSIGN_OR_RAISE(auto hns_support,
                        impl_->HierarchicalNamespaceSupport(adlfs_client));
  if (hns_support == HNSSupport::kContainerNotFound) {
    return PathNotFound(location);
  }
  if (hns_support == HNSSupport::kEnabled) {
    return impl_->DeleteDirOnFileSystem(adlfs_client, location);
  }
  DCHECK_EQ(hns_support, HNSSupport::kDisabled);
  auto container_client = impl_->GetBlobContainerClient(location.container);
  return impl_->DeleteDirContentsOnContainer(container_client, location,
                                             /*require_dir_to_exist=*/true,
                                             /*preserve_dir_marker_blob=*/false,
                                             "DeleteDir");
}

Status AzureFileSystem::DeleteDirContents(const std::string& path, bool missing_dir_ok) {
  ARROW_ASSIGN_OR_RAISE(auto location, AzureLocation::FromString(path));
  if (location.container.empty()) {
    return internal::InvalidDeleteDirContents(location.all);
  }

  auto adlfs_client = impl_->GetFileSystemClient(location.container);
  ARROW_ASSIGN_OR_RAISE(auto hns_support,
                        impl_->HierarchicalNamespaceSupport(adlfs_client));
  if (hns_support == HNSSupport::kContainerNotFound) {
    return missing_dir_ok ? Status::OK() : PathNotFound(location);
  }

  if (hns_support == HNSSupport::kEnabled) {
    return impl_->DeleteDirContentsOnFileSystem(adlfs_client, location, missing_dir_ok);
  }
  auto container_client = impl_->GetBlobContainerClient(location.container);
  return impl_->DeleteDirContentsOnContainer(container_client, location,
                                             /*require_dir_to_exist=*/!missing_dir_ok,
                                             /*preserve_dir_marker_blob=*/true,
                                             "DeleteDirContents");
}

Status AzureFileSystem::DeleteRootDirContents() {
  return Status::NotImplemented("Cannot delete all Azure Blob Storage containers");
}

Status AzureFileSystem::DeleteFile(const std::string& path) {
  return Status::NotImplemented("The Azure FileSystem is not fully implemented");
}

Status AzureFileSystem::Move(const std::string& src, const std::string& dest) {
  return Status::NotImplemented("The Azure FileSystem is not fully implemented");
}

Status AzureFileSystem::CopyFile(const std::string& src, const std::string& dest) {
  ARROW_ASSIGN_OR_RAISE(auto src_location, AzureLocation::FromString(src));
  ARROW_ASSIGN_OR_RAISE(auto dest_location, AzureLocation::FromString(dest));
  return impl_->CopyFile(src_location, dest_location);
}

Result<std::shared_ptr<io::InputStream>> AzureFileSystem::OpenInputStream(
    const std::string& path) {
  ARROW_ASSIGN_OR_RAISE(auto location, AzureLocation::FromString(path));
  return impl_->OpenInputFile(location, this);
}

Result<std::shared_ptr<io::InputStream>> AzureFileSystem::OpenInputStream(
    const FileInfo& info) {
  return impl_->OpenInputFile(info, this);
}

Result<std::shared_ptr<io::RandomAccessFile>> AzureFileSystem::OpenInputFile(
    const std::string& path) {
  ARROW_ASSIGN_OR_RAISE(auto location, AzureLocation::FromString(path));
  return impl_->OpenInputFile(location, this);
}

Result<std::shared_ptr<io::RandomAccessFile>> AzureFileSystem::OpenInputFile(
    const FileInfo& info) {
  return impl_->OpenInputFile(info, this);
}

Result<std::shared_ptr<io::OutputStream>> AzureFileSystem::OpenOutputStream(
    const std::string& path, const std::shared_ptr<const KeyValueMetadata>& metadata) {
  ARROW_ASSIGN_OR_RAISE(auto location, AzureLocation::FromString(path));
  return impl_->OpenAppendStream(location, metadata, true, this);
}

Result<std::shared_ptr<io::OutputStream>> AzureFileSystem::OpenAppendStream(
    const std::string& path, const std::shared_ptr<const KeyValueMetadata>& metadata) {
  ARROW_ASSIGN_OR_RAISE(auto location, AzureLocation::FromString(path));
  return impl_->OpenAppendStream(location, metadata, false, this);
}

}  // namespace arrow::fs
