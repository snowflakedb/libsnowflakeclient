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

#include "arrow/flight/client.h"

// Platform-specific defines
#include "arrow/flight/platform.h"

#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

#ifdef GRPCPP_PP_INCLUDE
#include <grpcpp/grpcpp.h>
#else
#include <grpc++/grpc++.h>
#endif

#include "arrow/buffer.h"
#include "arrow/ipc/reader.h"
#include "arrow/ipc/writer.h"
#include "arrow/memory_pool.h"
#include "arrow/record_batch.h"
#include "arrow/result.h"
#include "arrow/status.h"
#include "arrow/type.h"
#include "arrow/util/logging.h"
#include "arrow/util/uri.h"

#include "arrow/flight/client_auth.h"
#include "arrow/flight/client_middleware.h"
#include "arrow/flight/internal.h"
#include "arrow/flight/middleware.h"
#include "arrow/flight/middleware_internal.h"
#include "arrow/flight/serialization_internal.h"
#include "arrow/flight/types.h"

namespace pb = arrow::flight::protocol;

namespace arrow {

class MemoryPool;

namespace flight {

FlightCallOptions::FlightCallOptions() : timeout(-1) {}

struct ClientRpc {
  grpc::ClientContext context;

  explicit ClientRpc(const FlightCallOptions& options) {
    if (options.timeout.count() >= 0) {
      std::chrono::system_clock::time_point deadline =
          std::chrono::time_point_cast<std::chrono::system_clock::time_point::duration>(
              std::chrono::system_clock::now() + options.timeout);
      context.set_deadline(deadline);
    }
  }

  Status IOError(const std::string& error_message) {
    std::stringstream ss;
    ss << error_message << context.debug_error_string();
    return Status::IOError(ss.str());
  }

  /// \brief Add an auth token via an auth handler
  Status SetToken(ClientAuthHandler* auth_handler) {
    if (auth_handler) {
      std::string token;
      RETURN_NOT_OK(auth_handler->GetToken(&token));
      context.AddMetadata(internal::kGrpcAuthHeader, token);
    }
    return Status::OK();
  }
};

class GrpcAddCallHeaders : public AddCallHeaders {
 public:
  explicit GrpcAddCallHeaders(std::multimap<grpc::string, grpc::string>* metadata)
      : metadata_(metadata) {}
  ~GrpcAddCallHeaders() override = default;

  void AddHeader(const std::string& key, const std::string& value) override {
    metadata_->insert(std::make_pair(key, value));
  }

 private:
  std::multimap<grpc::string, grpc::string>* metadata_;
};

class GrpcClientInterceptorAdapter : public grpc::experimental::Interceptor {
 public:
  explicit GrpcClientInterceptorAdapter(
      std::vector<std::unique_ptr<ClientMiddleware>> middleware)
      : middleware_(std::move(middleware)) {}

  void Intercept(grpc::experimental::InterceptorBatchMethods* methods) {
    using InterceptionHookPoints = grpc::experimental::InterceptionHookPoints;
    if (methods->QueryInterceptionHookPoint(
            InterceptionHookPoints::PRE_SEND_INITIAL_METADATA)) {
      GrpcAddCallHeaders add_headers(methods->GetSendInitialMetadata());
      for (const auto& middleware : middleware_) {
        middleware->SendingHeaders(&add_headers);
      }
    }

    // TODO: also check for trailing metadata if call failed?
    // (grpc-java and grpc-core differ on this point, it seems)
    if (methods->QueryInterceptionHookPoint(
            InterceptionHookPoints::POST_RECV_INITIAL_METADATA)) {
      CallHeaders headers;
      for (const auto& entry : *methods->GetRecvInitialMetadata()) {
        headers.insert({util::string_view(entry.first.data(), entry.first.length()),
                        util::string_view(entry.second.data(), entry.second.length())});
      }
      for (const auto& middleware : middleware_) {
        middleware->ReceivedHeaders(headers);
      }
    }

    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::POST_RECV_STATUS)) {
      DCHECK_NE(nullptr, methods->GetRecvStatus());
      const Status status = internal::FromGrpcStatus(*methods->GetRecvStatus());

      for (const auto& middleware : middleware_) {
        middleware->CallCompleted(status);
      }
    }

    methods->Proceed();
  }

 private:
  std::vector<std::unique_ptr<ClientMiddleware>> middleware_;
};

class GrpcClientInterceptorAdapterFactory
    : public grpc::experimental::ClientInterceptorFactoryInterface {
 public:
  GrpcClientInterceptorAdapterFactory(
      std::vector<std::shared_ptr<ClientMiddlewareFactory>> middleware)
      : middleware_(middleware) {}

  grpc::experimental::Interceptor* CreateClientInterceptor(
      grpc::experimental::ClientRpcInfo* info) override {
    std::vector<std::unique_ptr<ClientMiddleware>> middleware;

    FlightMethod flight_method = FlightMethod::Invalid;
    util::string_view method(info->method());
    if (method.ends_with("/Handshake")) {
      flight_method = FlightMethod::Handshake;
    } else if (method.ends_with("/ListFlights")) {
      flight_method = FlightMethod::ListFlights;
    } else if (method.ends_with("/GetFlightInfo")) {
      flight_method = FlightMethod::GetFlightInfo;
    } else if (method.ends_with("/GetSchema")) {
      flight_method = FlightMethod::GetSchema;
    } else if (method.ends_with("/DoGet")) {
      flight_method = FlightMethod::DoGet;
    } else if (method.ends_with("/DoPut")) {
      flight_method = FlightMethod::DoPut;
    } else if (method.ends_with("/DoAction")) {
      flight_method = FlightMethod::DoAction;
    } else if (method.ends_with("/ListActions")) {
      flight_method = FlightMethod::ListActions;
    } else {
      DCHECK(false) << "Unknown Flight method: " << info->method();
    }

    const CallInfo flight_info{flight_method};
    for (auto& factory : middleware_) {
      std::unique_ptr<ClientMiddleware> instance;
      factory->StartCall(flight_info, &instance);
      if (instance) {
        middleware.push_back(std::move(instance));
      }
    }
    return new GrpcClientInterceptorAdapter(std::move(middleware));
  }

 private:
  std::vector<std::shared_ptr<ClientMiddlewareFactory>> middleware_;
};

class GrpcClientAuthSender : public ClientAuthSender {
 public:
  explicit GrpcClientAuthSender(
      std::shared_ptr<
          grpc::ClientReaderWriter<pb::HandshakeRequest, pb::HandshakeResponse>>
          stream)
      : stream_(stream) {}

  Status Write(const std::string& token) override {
    pb::HandshakeRequest response;
    response.set_payload(token);
    if (stream_->Write(response)) {
      return Status::OK();
    }
    return internal::FromGrpcStatus(stream_->Finish());
  }

 private:
  std::shared_ptr<grpc::ClientReaderWriter<pb::HandshakeRequest, pb::HandshakeResponse>>
      stream_;
};

class GrpcClientAuthReader : public ClientAuthReader {
 public:
  explicit GrpcClientAuthReader(
      std::shared_ptr<
          grpc::ClientReaderWriter<pb::HandshakeRequest, pb::HandshakeResponse>>
          stream)
      : stream_(stream) {}

  Status Read(std::string* token) override {
    pb::HandshakeResponse request;
    if (stream_->Read(&request)) {
      *token = std::move(*request.mutable_payload());
      return Status::OK();
    }
    return internal::FromGrpcStatus(stream_->Finish());
  }

 private:
  std::shared_ptr<grpc::ClientReaderWriter<pb::HandshakeRequest, pb::HandshakeResponse>>
      stream_;
};

// The next two classes are intertwined. To get the application
// metadata while avoiding reimplementing RecordBatchStreamReader, we
// create an ipc::MessageReader that is tied to the
// MetadataRecordBatchReader. Every time an IPC message is read, it updates
// the application metadata field of the MetadataRecordBatchReader. The
// MetadataRecordBatchReader wraps RecordBatchStreamReader, offering an
// additional method to get both the record batch and application
// metadata.

class GrpcIpcMessageReader;
class GrpcStreamReader : public FlightStreamReader {
 public:
  GrpcStreamReader();

  static Status Open(std::unique_ptr<ClientRpc> rpc,
                     std::unique_ptr<grpc::ClientReader<pb::FlightData>> stream,
                     std::unique_ptr<GrpcStreamReader>* out);
  std::shared_ptr<Schema> schema() const override;
  Status Next(FlightStreamChunk* out) override;
  void Cancel() override;

 private:
  friend class GrpcIpcMessageReader;
  std::shared_ptr<ipc::RecordBatchReader> batch_reader_;
  std::shared_ptr<Buffer> last_app_metadata_;
  std::shared_ptr<ClientRpc> rpc_;
};

class GrpcIpcMessageReader : public ipc::MessageReader {
 public:
  GrpcIpcMessageReader(GrpcStreamReader* reader, std::shared_ptr<ClientRpc> rpc,
                       std::unique_ptr<grpc::ClientReader<pb::FlightData>> stream)
      : flight_reader_(reader),
        rpc_(rpc),
        stream_(std::move(stream)),
        stream_finished_(false) {}

  ::arrow::Result<std::unique_ptr<ipc::Message>> ReadNextMessage() override {
    std::unique_ptr<ipc::Message> out;
    RETURN_NOT_OK(GetNextMessage(&out));
    return std::move(out);
  }

 protected:
  Status GetNextMessage(std::unique_ptr<ipc::Message>* out) {
    // TODO: Use Result APIs
    if (stream_finished_) {
      *out = nullptr;
      flight_reader_->last_app_metadata_ = nullptr;
      return Status::OK();
    }
    internal::FlightData data;
    if (!internal::ReadPayload(stream_.get(), &data)) {
      // Stream is completed
      stream_finished_ = true;
      *out = nullptr;
      flight_reader_->last_app_metadata_ = nullptr;
      return OverrideWithServerError(Status::OK());
    }
    // Validate IPC message
    auto st = data.OpenMessage(out);
    if (!st.ok()) {
      flight_reader_->last_app_metadata_ = nullptr;
      return OverrideWithServerError(std::move(st));
    }
    flight_reader_->last_app_metadata_ = data.app_metadata;
    return Status::OK();
  }

  Status OverrideWithServerError(Status&& st) {
    // Get the gRPC status if not OK, to propagate any server error message
    RETURN_NOT_OK(internal::FromGrpcStatus(stream_->Finish(), &rpc_->context));
    return std::move(st);
  }

 private:
  GrpcStreamReader* flight_reader_;
  // The RPC context lifetime must be coupled to the ClientReader
  std::shared_ptr<ClientRpc> rpc_;
  std::unique_ptr<grpc::ClientReader<pb::FlightData>> stream_;
  bool stream_finished_;
};

GrpcStreamReader::GrpcStreamReader() {}

Status GrpcStreamReader::Open(std::unique_ptr<ClientRpc> rpc,
                              std::unique_ptr<grpc::ClientReader<pb::FlightData>> stream,
                              std::unique_ptr<GrpcStreamReader>* out) {
  *out = std::unique_ptr<GrpcStreamReader>(new GrpcStreamReader);
  out->get()->rpc_ = std::move(rpc);
  auto reader = std::unique_ptr<ipc::MessageReader>(
      new GrpcIpcMessageReader(out->get(), out->get()->rpc_, std::move(stream)));
  ARROW_ASSIGN_OR_RAISE((*out)->batch_reader_,
                        ipc::RecordBatchStreamReader::Open(std::move(reader)));
  return Status::OK();
}

std::shared_ptr<Schema> GrpcStreamReader::schema() const {
  return batch_reader_->schema();
}

Status GrpcStreamReader::Next(FlightStreamChunk* out) {
  out->app_metadata = nullptr;
  RETURN_NOT_OK(batch_reader_->ReadNext(&out->data));
  out->app_metadata = std::move(last_app_metadata_);
  return Status::OK();
}

void GrpcStreamReader::Cancel() { rpc_->context.TryCancel(); }

// Similarly, the next two classes are intertwined. In order to get
// application-specific metadata to the IpcPayloadWriter,
// DoPutPayloadWriter takes a pointer to
// GrpcStreamWriter. GrpcStreamWriter updates a metadata field on
// write; DoPutPayloadWriter reads that metadata field to determine
// what to write.

class DoPutPayloadWriter;
class GrpcStreamWriter : public FlightStreamWriter {
 public:
  ~GrpcStreamWriter() override = default;

  explicit GrpcStreamWriter(
      std::shared_ptr<grpc::ClientReaderWriter<pb::FlightData, pb::PutResult>> writer)
      : app_metadata_(nullptr), batch_writer_(nullptr), writer_(writer) {}

  static Status Open(
      const FlightDescriptor& descriptor, const std::shared_ptr<Schema>& schema,
      std::unique_ptr<ClientRpc> rpc, std::unique_ptr<pb::PutResult> response,
      std::shared_ptr<std::mutex> read_mutex,
      std::shared_ptr<grpc::ClientReaderWriter<pb::FlightData, pb::PutResult>> writer,
      std::unique_ptr<FlightStreamWriter>* out);

  Status WriteRecordBatch(const RecordBatch& batch) override {
    return WriteWithMetadata(batch, nullptr);
  }
  Status WriteWithMetadata(const RecordBatch& batch,
                           std::shared_ptr<Buffer> app_metadata) override {
    app_metadata_ = app_metadata;
    return batch_writer_->WriteRecordBatch(batch);
  }
  Status DoneWriting() override {
    if (done_writing_) {
      return Status::OK();
    }
    done_writing_ = true;
    if (!writer_->WritesDone()) {
      return Status::IOError("Could not flush pending record batches.");
    }
    return Status::OK();
  }
  Status Close() override { return batch_writer_->Close(); }

 private:
  friend class DoPutPayloadWriter;
  std::shared_ptr<Buffer> app_metadata_;
  std::unique_ptr<ipc::RecordBatchWriter> batch_writer_;
  std::shared_ptr<grpc::ClientReaderWriter<pb::FlightData, pb::PutResult>> writer_;
  bool done_writing_ = false;
};

/// A IpcPayloadWriter implementation that writes to a DoPut stream
class DoPutPayloadWriter : public ipc::internal::IpcPayloadWriter {
 public:
  DoPutPayloadWriter(
      const FlightDescriptor& descriptor, std::unique_ptr<ClientRpc> rpc,
      std::unique_ptr<pb::PutResult> response, std::shared_ptr<std::mutex> read_mutex,
      std::shared_ptr<grpc::ClientReaderWriter<pb::FlightData, pb::PutResult>> writer,
      GrpcStreamWriter* stream_writer)
      : descriptor_(descriptor),
        rpc_(std::move(rpc)),
        response_(std::move(response)),
        read_mutex_(read_mutex),
        writer_(std::move(writer)),
        first_payload_(true),
        stream_writer_(stream_writer) {}

  ~DoPutPayloadWriter() override = default;

  Status Start() override { return Status::OK(); }

  Status WritePayload(const ipc::internal::IpcPayload& ipc_payload) override {
    FlightPayload payload;
    payload.ipc_message = ipc_payload;

    if (first_payload_) {
      // First Flight message needs to encore the Flight descriptor
      if (ipc_payload.type != ipc::Message::SCHEMA) {
        return Status::Invalid("First IPC message should be schema");
      }
      std::string str_descr;
      {
        pb::FlightDescriptor pb_descr;
        RETURN_NOT_OK(internal::ToProto(descriptor_, &pb_descr));
        if (!pb_descr.SerializeToString(&str_descr)) {
          return Status::UnknownError("Failed to serialized Flight descriptor");
        }
      }
      payload.descriptor = Buffer::FromString(std::move(str_descr));
      first_payload_ = false;
    } else if (ipc_payload.type == ipc::Message::RECORD_BATCH &&
               stream_writer_->app_metadata_) {
      payload.app_metadata = std::move(stream_writer_->app_metadata_);
    }

    if (!internal::WritePayload(payload, writer_.get())) {
      return rpc_->IOError("Could not write record batch to stream: ");
    }
    return Status::OK();
  }

  Status Close() override {
    bool finished_writes = stream_writer_->done_writing_ ? true : writer_->WritesDone();
    // Drain the read side to avoid hanging
    std::unique_lock<std::mutex> guard(*read_mutex_, std::try_to_lock);
    if (!guard.owns_lock()) {
      return Status::IOError("Cannot close stream with pending read operation.");
    }
    pb::PutResult message;
    while (writer_->Read(&message)) {
    }
    RETURN_NOT_OK(internal::FromGrpcStatus(writer_->Finish(), &rpc_->context));
    if (!finished_writes) {
      return Status::UnknownError(
          "Could not finish writing record batches before closing");
    }
    return Status::OK();
  }

 protected:
  // TODO: there isn't a way to access this as a user.
  const FlightDescriptor descriptor_;
  std::unique_ptr<ClientRpc> rpc_;
  std::unique_ptr<pb::PutResult> response_;
  std::shared_ptr<std::mutex> read_mutex_;
  std::shared_ptr<grpc::ClientReaderWriter<pb::FlightData, pb::PutResult>> writer_;
  bool first_payload_;
  GrpcStreamWriter* stream_writer_;
};

Status GrpcStreamWriter::Open(
    const FlightDescriptor& descriptor, const std::shared_ptr<Schema>& schema,
    std::unique_ptr<ClientRpc> rpc, std::unique_ptr<pb::PutResult> response,
    std::shared_ptr<std::mutex> read_mutex,
    std::shared_ptr<grpc::ClientReaderWriter<pb::FlightData, pb::PutResult>> writer,
    std::unique_ptr<FlightStreamWriter>* out) {
  std::unique_ptr<GrpcStreamWriter> result(new GrpcStreamWriter(writer));
  std::unique_ptr<ipc::internal::IpcPayloadWriter> payload_writer(new DoPutPayloadWriter(
      descriptor, std::move(rpc), std::move(response), read_mutex, writer, result.get()));
  ARROW_ASSIGN_OR_RAISE(result->batch_writer_, ipc::internal::OpenRecordBatchWriter(
                                                   std::move(payload_writer), schema));
  *out = std::move(result);
  return Status::OK();
}

FlightMetadataReader::~FlightMetadataReader() = default;

class GrpcMetadataReader : public FlightMetadataReader {
 public:
  explicit GrpcMetadataReader(
      std::shared_ptr<grpc::ClientReaderWriter<pb::FlightData, pb::PutResult>> reader,
      std::shared_ptr<std::mutex> read_mutex)
      : reader_(reader), read_mutex_(read_mutex) {}

  Status ReadMetadata(std::shared_ptr<Buffer>* out) override {
    std::lock_guard<std::mutex> guard(*read_mutex_);
    pb::PutResult message;
    if (reader_->Read(&message)) {
      *out = Buffer::FromString(std::move(*message.mutable_app_metadata()));
    } else {
      // Stream finished
      *out = nullptr;
    }
    return Status::OK();
  }

 private:
  std::shared_ptr<grpc::ClientReaderWriter<pb::FlightData, pb::PutResult>> reader_;
  std::shared_ptr<std::mutex> read_mutex_;
};

class FlightClient::FlightClientImpl {
 public:
  Status Connect(const Location& location, const FlightClientOptions& options) {
    const std::string& scheme = location.scheme();

    std::stringstream grpc_uri;
    std::shared_ptr<grpc::ChannelCredentials> creds;
    if (scheme == kSchemeGrpc || scheme == kSchemeGrpcTcp || scheme == kSchemeGrpcTls) {
      grpc_uri << location.uri_->host() << ":" << location.uri_->port_text();

      if (scheme == "grpc+tls") {
        grpc::SslCredentialsOptions ssl_options;
        if (!options.tls_root_certs.empty()) {
          ssl_options.pem_root_certs = options.tls_root_certs;
        }
        creds = grpc::SslCredentials(ssl_options);
      } else {
        creds = grpc::InsecureChannelCredentials();
      }
    } else if (scheme == kSchemeGrpcUnix) {
      grpc_uri << "unix://" << location.uri_->path();
      creds = grpc::InsecureChannelCredentials();
    } else {
      return Status::NotImplemented("Flight scheme " + scheme + " is not supported.");
    }

    grpc::ChannelArguments args;
    // Try to reconnect quickly at first, in case the server is still starting up
    args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 100);
    // Receive messages of any size
    args.SetMaxReceiveMessageSize(-1);

    if (options.override_hostname != "") {
      args.SetSslTargetNameOverride(options.override_hostname);
    }

    std::vector<std::unique_ptr<grpc::experimental::ClientInterceptorFactoryInterface>>
        interceptors;
    interceptors.emplace_back(
        new GrpcClientInterceptorAdapterFactory(std::move(options.middleware)));

    stub_ = pb::FlightService::NewStub(
        grpc::experimental::CreateCustomChannelWithInterceptors(
            grpc_uri.str(), creds, args, std::move(interceptors)));

    return Status::OK();
  }

  Status Authenticate(const FlightCallOptions& options,
                      std::unique_ptr<ClientAuthHandler> auth_handler) {
    auth_handler_ = std::move(auth_handler);
    ClientRpc rpc(options);
    std::shared_ptr<grpc::ClientReaderWriter<pb::HandshakeRequest, pb::HandshakeResponse>>
        stream = stub_->Handshake(&rpc.context);
    GrpcClientAuthSender outgoing{stream};
    GrpcClientAuthReader incoming{stream};
    RETURN_NOT_OK(auth_handler_->Authenticate(&outgoing, &incoming));
    // Explicitly close our side of the connection
    bool finished_writes = stream->WritesDone();
    RETURN_NOT_OK(internal::FromGrpcStatus(stream->Finish(), &rpc.context));
    if (!finished_writes) {
      return Status::UnknownError("Could not finish writing before closing");
    }
    return Status::OK();
  }

  Status ListFlights(const FlightCallOptions& options, const Criteria& criteria,
                     std::unique_ptr<FlightListing>* listing) {
    pb::Criteria pb_criteria;
    RETURN_NOT_OK(internal::ToProto(criteria, &pb_criteria));

    ClientRpc rpc(options);
    RETURN_NOT_OK(rpc.SetToken(auth_handler_.get()));
    std::unique_ptr<grpc::ClientReader<pb::FlightInfo>> stream(
        stub_->ListFlights(&rpc.context, pb_criteria));

    std::vector<FlightInfo> flights;

    pb::FlightInfo pb_info;
    while (stream->Read(&pb_info)) {
      FlightInfo::Data info_data;
      RETURN_NOT_OK(internal::FromProto(pb_info, &info_data));
      flights.emplace_back(std::move(info_data));
    }

    listing->reset(new SimpleFlightListing(std::move(flights)));
    return internal::FromGrpcStatus(stream->Finish(), &rpc.context);
  }

  Status DoAction(const FlightCallOptions& options, const Action& action,
                  std::unique_ptr<ResultStream>* results) {
    pb::Action pb_action;
    RETURN_NOT_OK(internal::ToProto(action, &pb_action));

    ClientRpc rpc(options);
    RETURN_NOT_OK(rpc.SetToken(auth_handler_.get()));
    std::unique_ptr<grpc::ClientReader<pb::Result>> stream(
        stub_->DoAction(&rpc.context, pb_action));

    pb::Result pb_result;

    std::vector<Result> materialized_results;
    while (stream->Read(&pb_result)) {
      Result result;
      RETURN_NOT_OK(internal::FromProto(pb_result, &result));
      materialized_results.emplace_back(std::move(result));
    }

    *results = std::unique_ptr<ResultStream>(
        new SimpleResultStream(std::move(materialized_results)));
    return internal::FromGrpcStatus(stream->Finish(), &rpc.context);
  }

  Status ListActions(const FlightCallOptions& options, std::vector<ActionType>* types) {
    pb::Empty empty;

    ClientRpc rpc(options);
    RETURN_NOT_OK(rpc.SetToken(auth_handler_.get()));
    std::unique_ptr<grpc::ClientReader<pb::ActionType>> stream(
        stub_->ListActions(&rpc.context, empty));

    pb::ActionType pb_type;
    ActionType type;
    while (stream->Read(&pb_type)) {
      RETURN_NOT_OK(internal::FromProto(pb_type, &type));
      types->emplace_back(std::move(type));
    }
    return internal::FromGrpcStatus(stream->Finish(), &rpc.context);
  }

  Status GetFlightInfo(const FlightCallOptions& options,
                       const FlightDescriptor& descriptor,
                       std::unique_ptr<FlightInfo>* info) {
    pb::FlightDescriptor pb_descriptor;
    pb::FlightInfo pb_response;

    RETURN_NOT_OK(internal::ToProto(descriptor, &pb_descriptor));

    ClientRpc rpc(options);
    RETURN_NOT_OK(rpc.SetToken(auth_handler_.get()));
    Status s = internal::FromGrpcStatus(
        stub_->GetFlightInfo(&rpc.context, pb_descriptor, &pb_response), &rpc.context);
    RETURN_NOT_OK(s);

    FlightInfo::Data info_data;
    RETURN_NOT_OK(internal::FromProto(pb_response, &info_data));
    info->reset(new FlightInfo(std::move(info_data)));
    return Status::OK();
  }

  Status GetSchema(const FlightCallOptions& options, const FlightDescriptor& descriptor,
                   std::unique_ptr<SchemaResult>* schema_result) {
    pb::FlightDescriptor pb_descriptor;
    pb::SchemaResult pb_response;

    RETURN_NOT_OK(internal::ToProto(descriptor, &pb_descriptor));

    ClientRpc rpc(options);
    RETURN_NOT_OK(rpc.SetToken(auth_handler_.get()));
    Status s = internal::FromGrpcStatus(
        stub_->GetSchema(&rpc.context, pb_descriptor, &pb_response), &rpc.context);
    RETURN_NOT_OK(s);

    std::string str;
    RETURN_NOT_OK(internal::FromProto(pb_response, &str));
    schema_result->reset(new SchemaResult(str));
    return Status::OK();
  }

  Status DoGet(const FlightCallOptions& options, const Ticket& ticket,
               std::unique_ptr<FlightStreamReader>* out) {
    pb::Ticket pb_ticket;
    internal::ToProto(ticket, &pb_ticket);

    std::unique_ptr<ClientRpc> rpc(new ClientRpc(options));
    RETURN_NOT_OK(rpc->SetToken(auth_handler_.get()));
    std::unique_ptr<grpc::ClientReader<pb::FlightData>> stream(
        stub_->DoGet(&rpc->context, pb_ticket));

    std::unique_ptr<GrpcStreamReader> reader;
    RETURN_NOT_OK(GrpcStreamReader::Open(std::move(rpc), std::move(stream), &reader));
    *out = std::move(reader);
    return Status::OK();
  }

  Status DoPut(const FlightCallOptions& options, const FlightDescriptor& descriptor,
               const std::shared_ptr<Schema>& schema,
               std::unique_ptr<FlightStreamWriter>* out,
               std::unique_ptr<FlightMetadataReader>* reader) {
    std::unique_ptr<ClientRpc> rpc(new ClientRpc(options));
    RETURN_NOT_OK(rpc->SetToken(auth_handler_.get()));
    std::unique_ptr<pb::PutResult> response(new pb::PutResult);
    std::shared_ptr<grpc::ClientReaderWriter<pb::FlightData, pb::PutResult>> writer(
        stub_->DoPut(&rpc->context));

    std::shared_ptr<std::mutex> read_mutex = std::make_shared<std::mutex>();
    *reader =
        std::unique_ptr<FlightMetadataReader>(new GrpcMetadataReader(writer, read_mutex));
    return GrpcStreamWriter::Open(descriptor, schema, std::move(rpc), std::move(response),
                                  read_mutex, writer, out);
  }

 private:
  std::unique_ptr<pb::FlightService::Stub> stub_;
  std::shared_ptr<ClientAuthHandler> auth_handler_;
};

FlightClient::FlightClient() { impl_.reset(new FlightClientImpl); }

FlightClient::~FlightClient() {}

Status FlightClient::Connect(const Location& location,
                             std::unique_ptr<FlightClient>* client) {
  return Connect(location, {}, client);
}

Status FlightClient::Connect(const Location& location, const FlightClientOptions& options,
                             std::unique_ptr<FlightClient>* client) {
  client->reset(new FlightClient);
  return (*client)->impl_->Connect(location, options);
}

Status FlightClient::Authenticate(const FlightCallOptions& options,
                                  std::unique_ptr<ClientAuthHandler> auth_handler) {
  return impl_->Authenticate(options, std::move(auth_handler));
}

Status FlightClient::DoAction(const FlightCallOptions& options, const Action& action,
                              std::unique_ptr<ResultStream>* results) {
  return impl_->DoAction(options, action, results);
}

Status FlightClient::ListActions(const FlightCallOptions& options,
                                 std::vector<ActionType>* actions) {
  return impl_->ListActions(options, actions);
}

Status FlightClient::GetFlightInfo(const FlightCallOptions& options,
                                   const FlightDescriptor& descriptor,
                                   std::unique_ptr<FlightInfo>* info) {
  return impl_->GetFlightInfo(options, descriptor, info);
}

Status FlightClient::GetSchema(const FlightCallOptions& options,
                               const FlightDescriptor& descriptor,
                               std::unique_ptr<SchemaResult>* schema_result) {
  return impl_->GetSchema(options, descriptor, schema_result);
}

Status FlightClient::ListFlights(std::unique_ptr<FlightListing>* listing) {
  return ListFlights({}, {}, listing);
}

Status FlightClient::ListFlights(const FlightCallOptions& options,
                                 const Criteria& criteria,
                                 std::unique_ptr<FlightListing>* listing) {
  return impl_->ListFlights(options, criteria, listing);
}

Status FlightClient::DoGet(const FlightCallOptions& options, const Ticket& ticket,
                           std::unique_ptr<FlightStreamReader>* stream) {
  return impl_->DoGet(options, ticket, stream);
}

Status FlightClient::DoPut(const FlightCallOptions& options,
                           const FlightDescriptor& descriptor,
                           const std::shared_ptr<Schema>& schema,
                           std::unique_ptr<FlightStreamWriter>* stream,
                           std::unique_ptr<FlightMetadataReader>* reader) {
  return impl_->DoPut(options, descriptor, schema, stream, reader);
}

}  // namespace flight
}  // namespace arrow
