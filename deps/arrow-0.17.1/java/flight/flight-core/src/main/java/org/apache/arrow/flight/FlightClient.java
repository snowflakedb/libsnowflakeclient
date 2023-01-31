/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.arrow.flight;

import java.io.InputStream;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.TimeUnit;

import javax.net.ssl.SSLException;

import org.apache.arrow.flight.FlightProducer.StreamListener;
import org.apache.arrow.flight.auth.BasicClientAuthHandler;
import org.apache.arrow.flight.auth.ClientAuthHandler;
import org.apache.arrow.flight.auth.ClientAuthInterceptor;
import org.apache.arrow.flight.auth.ClientAuthWrapper;
import org.apache.arrow.flight.grpc.ClientInterceptorAdapter;
import org.apache.arrow.flight.grpc.StatusUtils;
import org.apache.arrow.flight.impl.Flight;
import org.apache.arrow.flight.impl.Flight.Empty;
import org.apache.arrow.flight.impl.FlightServiceGrpc;
import org.apache.arrow.flight.impl.FlightServiceGrpc.FlightServiceBlockingStub;
import org.apache.arrow.flight.impl.FlightServiceGrpc.FlightServiceStub;
import org.apache.arrow.memory.BufferAllocator;
import org.apache.arrow.util.Preconditions;
import org.apache.arrow.vector.VectorSchemaRoot;
import org.apache.arrow.vector.VectorUnloader;
import org.apache.arrow.vector.dictionary.DictionaryProvider;
import org.apache.arrow.vector.dictionary.DictionaryProvider.MapDictionaryProvider;
import org.apache.arrow.vector.ipc.message.ArrowRecordBatch;

import io.grpc.Channel;
import io.grpc.ClientCall;
import io.grpc.ClientInterceptor;
import io.grpc.ClientInterceptors;
import io.grpc.ManagedChannel;
import io.grpc.MethodDescriptor;
import io.grpc.StatusRuntimeException;
import io.grpc.netty.GrpcSslContexts;
import io.grpc.netty.NettyChannelBuilder;
import io.grpc.stub.ClientCallStreamObserver;
import io.grpc.stub.ClientCalls;
import io.grpc.stub.ClientResponseObserver;
import io.grpc.stub.StreamObserver;
import io.netty.buffer.ArrowBuf;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.ServerChannel;
import io.netty.handler.ssl.SslContextBuilder;

/**
 * Client for Flight services.
 */
public class FlightClient implements AutoCloseable {
  private static final int PENDING_REQUESTS = 5;
  /** The maximum number of trace events to keep on the gRPC Channel. This value disables channel tracing. */
  private static final int MAX_CHANNEL_TRACE_EVENTS = 0;
  private final BufferAllocator allocator;
  private final ManagedChannel channel;
  private final Channel interceptedChannel;
  private final FlightServiceBlockingStub blockingStub;
  private final FlightServiceStub asyncStub;
  private final ClientAuthInterceptor authInterceptor = new ClientAuthInterceptor();
  private final MethodDescriptor<Flight.Ticket, ArrowMessage> doGetDescriptor;
  private final MethodDescriptor<ArrowMessage, Flight.PutResult> doPutDescriptor;

  /**
   * Create a Flight client from an allocator and a gRPC channel.
   */
  FlightClient(BufferAllocator incomingAllocator, ManagedChannel channel,
      List<FlightClientMiddleware.Factory> middleware) {
    this.allocator = incomingAllocator.newChildAllocator("flight-client", 0, Long.MAX_VALUE);
    this.channel = channel;

    final ClientInterceptor[] interceptors;
    interceptors = new ClientInterceptor[]{authInterceptor, new ClientInterceptorAdapter(middleware)};

    // Create a channel with interceptors pre-applied for DoGet and DoPut
    this.interceptedChannel = ClientInterceptors.intercept(channel, interceptors);

    blockingStub = FlightServiceGrpc.newBlockingStub(interceptedChannel);
    asyncStub = FlightServiceGrpc.newStub(interceptedChannel);
    doGetDescriptor = FlightBindingService.getDoGetDescriptor(allocator);
    doPutDescriptor = FlightBindingService.getDoPutDescriptor(allocator);
  }

  /**
   * Get a list of available flights.
   *
   * @param criteria Criteria for selecting flights
   * @param options RPC-layer hints for the call.
   * @return FlightInfo Iterable
   */
  public Iterable<FlightInfo> listFlights(Criteria criteria, CallOption... options) {
    final Iterator<Flight.FlightInfo> flights;
    try {
      flights = CallOptions.wrapStub(blockingStub, options)
          .listFlights(criteria.asCriteria());
    } catch (StatusRuntimeException sre) {
      throw StatusUtils.fromGrpcRuntimeException(sre);
    }
    return () -> StatusUtils.wrapIterator(flights, t -> {
      try {
        return new FlightInfo(t);
      } catch (URISyntaxException e) {
        // We don't expect this will happen for conforming Flight implementations. For instance, a Java server
        // itself wouldn't be able to construct an invalid Location.
        throw new RuntimeException(e);
      }
    });
  }

  /**
   * Lists actions available on the Flight service.
   *
   * @param options RPC-layer hints for the call.
   */
  public Iterable<ActionType> listActions(CallOption... options) {
    final Iterator<Flight.ActionType> actions;
    try {
      actions = CallOptions.wrapStub(blockingStub, options)
          .listActions(Empty.getDefaultInstance());
    } catch (StatusRuntimeException sre) {
      throw StatusUtils.fromGrpcRuntimeException(sre);
    }
    return () -> StatusUtils.wrapIterator(actions, ActionType::new);
  }

  /**
   * Performs an action on the Flight service.
   *
   * @param action The action to perform.
   * @param options RPC-layer hints for this call.
   * @return An iterator of results.
   */
  public Iterator<Result> doAction(Action action, CallOption... options) {
    return StatusUtils
        .wrapIterator(CallOptions.wrapStub(blockingStub, options).doAction(action.toProtocol()), Result::new);
  }

  /**
   * Authenticates with a username and password.
   */
  public void authenticateBasic(String username, String password) {
    BasicClientAuthHandler basicClient = new BasicClientAuthHandler(username, password);
    authenticate(basicClient);
  }

  /**
   * Authenticates against the Flight service.
   *
   * @param options RPC-layer hints for this call.
   * @param handler The auth mechanism to use.
   */
  public void authenticate(ClientAuthHandler handler, CallOption... options) {
    Preconditions.checkArgument(!authInterceptor.hasAuthHandler(), "Auth already completed.");
    ClientAuthWrapper.doClientAuth(handler, CallOptions.wrapStub(asyncStub, options));
    authInterceptor.setAuthHandler(handler);
  }

  /**
   * Create or append a descriptor with another stream.
   *
   * @param descriptor FlightDescriptor the descriptor for the data
   * @param root VectorSchemaRoot the root containing data
   * @param metadataListener A handler for metadata messages from the server. This will be passed buffers that will be
   *     freed after {@link StreamListener#onNext(Object)} is called!
   * @param options RPC-layer hints for this call.
   * @return ClientStreamListener an interface to control uploading data
   */
  public ClientStreamListener startPut(FlightDescriptor descriptor, VectorSchemaRoot root,
      PutListener metadataListener, CallOption... options) {
    return startPut(descriptor, root, new MapDictionaryProvider(), metadataListener, options);
  }

  /**
   * Create or append a descriptor with another stream.
   * @param descriptor FlightDescriptor the descriptor for the data
   * @param root VectorSchemaRoot the root containing data
   * @param metadataListener A handler for metadata messages from the server.
   * @param options RPC-layer hints for this call.
   * @return ClientStreamListener an interface to control uploading data
   */
  public ClientStreamListener startPut(FlightDescriptor descriptor, VectorSchemaRoot root, DictionaryProvider provider,
      PutListener metadataListener, CallOption... options) {
    Preconditions.checkNotNull(descriptor);
    Preconditions.checkNotNull(root);

    try {
      SetStreamObserver resultObserver = new SetStreamObserver(allocator, metadataListener);
      final io.grpc.CallOptions callOptions = CallOptions.wrapStub(asyncStub, options).getCallOptions();
      ClientCallStreamObserver<ArrowMessage> observer = (ClientCallStreamObserver<ArrowMessage>)
          ClientCalls.asyncBidiStreamingCall(
              interceptedChannel.newCall(doPutDescriptor, callOptions), resultObserver);
      // send the schema to start.
      DictionaryUtils.generateSchemaMessages(root.getSchema(), descriptor, provider, observer::onNext);
      return new PutObserver(new VectorUnloader(
          root, true /* include # of nulls in vectors */, true /* must align buffers to be C++-compatible */),
          observer, metadataListener);
    } catch (StatusRuntimeException sre) {
      throw StatusUtils.fromGrpcRuntimeException(sre);
    } catch (Exception e) {
      // Only happens if DictionaryUtils#generateSchemaMessages fails. This should only happen if closing buffers fails,
      // which means the application is in an unknown state, so propagate the exception.
      throw CallStatus.INTERNAL.withDescription("Could not send all schema messages: " + e.toString()).withCause(e)
          .toRuntimeException();
    }
  }

  /**
   * Get info on a stream.
   * @param descriptor The descriptor for the stream.
   * @param options RPC-layer hints for this call.
   */
  public FlightInfo getInfo(FlightDescriptor descriptor, CallOption... options) {
    try {
      return new FlightInfo(CallOptions.wrapStub(blockingStub, options).getFlightInfo(descriptor.toProtocol()));
    } catch (URISyntaxException e) {
      // We don't expect this will happen for conforming Flight implementations. For instance, a Java server
      // itself wouldn't be able to construct an invalid Location.
      throw new RuntimeException(e);
    } catch (StatusRuntimeException sre) {
      throw StatusUtils.fromGrpcRuntimeException(sre);
    }
  }

  /**
   * Get schema for a stream.
   * @param descriptor The descriptor for the stream.
   * @param options RPC-layer hints for this call.
   */
  public SchemaResult getSchema(FlightDescriptor descriptor, CallOption... options) {
    return SchemaResult.fromProtocol(CallOptions.wrapStub(blockingStub, options).getSchema(descriptor.toProtocol()));
  }

  /**
   * Retrieve a stream from the server.
   * @param ticket The ticket granting access to the data stream.
   * @param options RPC-layer hints for this call.
   */
  public FlightStream getStream(Ticket ticket, CallOption... options) {
    final io.grpc.CallOptions callOptions = CallOptions.wrapStub(asyncStub, options).getCallOptions();
    ClientCall<Flight.Ticket, ArrowMessage> call = interceptedChannel.newCall(doGetDescriptor, callOptions);
    FlightStream stream = new FlightStream(
        allocator,
        PENDING_REQUESTS,
        (String message, Throwable cause) -> call.cancel(message, cause),
        (count) -> call.request(count));

    final StreamObserver<ArrowMessage> delegate = stream.asObserver();
    ClientResponseObserver<Flight.Ticket, ArrowMessage> clientResponseObserver =
        new ClientResponseObserver<Flight.Ticket, ArrowMessage>() {

          @Override
          public void beforeStart(ClientCallStreamObserver<org.apache.arrow.flight.impl.Flight.Ticket> requestStream) {
            requestStream.disableAutoInboundFlowControl();
          }

          @Override
          public void onNext(ArrowMessage value) {
            delegate.onNext(value);
          }

          @Override
          public void onError(Throwable t) {
            delegate.onError(StatusUtils.toGrpcException(t));
          }

          @Override
          public void onCompleted() {
            delegate.onCompleted();
          }

        };

    ClientCalls.asyncServerStreamingCall(call, ticket.toProtocol(), clientResponseObserver);
    return stream;
  }

  private static class SetStreamObserver implements StreamObserver<Flight.PutResult> {
    private final BufferAllocator allocator;
    private final StreamListener<PutResult> listener;

    SetStreamObserver(BufferAllocator allocator, StreamListener<PutResult> listener) {
      super();
      this.allocator = allocator;
      this.listener = listener == null ? NoOpStreamListener.getInstance() : listener;
    }

    @Override
    public void onNext(Flight.PutResult value) {
      try (final PutResult message = PutResult.fromProtocol(allocator, value)) {
        listener.onNext(message);
      }
    }

    @Override
    public void onError(Throwable t) {
      listener.onError(StatusUtils.fromThrowable(t));
    }

    @Override
    public void onCompleted() {
      listener.onCompleted();
    }
  }

  private static class PutObserver implements ClientStreamListener {

    private final ClientCallStreamObserver<ArrowMessage> observer;
    private final VectorUnloader unloader;
    private final PutListener listener;

    public PutObserver(VectorUnloader unloader, ClientCallStreamObserver<ArrowMessage> observer,
        PutListener listener) {
      this.observer = observer;
      this.unloader = unloader;
      this.listener = listener;
    }

    @Override
    public void putNext() {
      putNext(null);
    }

    @Override
    public void putNext(ArrowBuf appMetadata) {
      ArrowRecordBatch batch = unloader.getRecordBatch();
      // Check isCancelled as well to avoid inadvertently blocking forever
      // (so long as PutListener properly implements it)
      while (!observer.isReady() && !listener.isCancelled()) {
        /* busy wait */
      }
      // ArrowMessage takes ownership of appMetadata and batch
      // gRPC should take ownership of ArrowMessage, but in some cases it doesn't, so guard against it
      // ArrowMessage#close is a no-op if gRPC did its job
      try (final ArrowMessage message = new ArrowMessage(batch, appMetadata)) {
        observer.onNext(message);
      } catch (Exception e) {
        throw StatusUtils.fromThrowable(e);
      }
    }

    @Override
    public void error(Throwable ex) {
      observer.onError(StatusUtils.toGrpcException(ex));
    }

    @Override
    public void completed() {
      observer.onCompleted();
    }

    @Override
    public void getResult() {
      listener.getResult();
    }
  }

  /**
   * Interface for subscribers to a stream returned by the server.
   */
  public interface ClientStreamListener {

    /**
     * Send the current data in the corresponding {@link VectorSchemaRoot} to the server.
     */
    void putNext();

    /**
     * Send the current data in the corresponding {@link VectorSchemaRoot} to the server, along with
     * application-specific metadata. This takes ownership of the buffer.
     */
    void putNext(ArrowBuf appMetadata);

    /**
     * Indicate an error to the server. Terminates the stream; do not call {@link #completed()}.
     */
    void error(Throwable ex);

    /** Indicate the stream is finished on the client side. */
    void completed();

    /**
     * Wait for the stream to finish on the server side. You must call this to be notified of any errors that may have
     * happened during the upload.
     */
    void getResult();
  }

  /**
   * A handler for server-sent application metadata messages during a Flight DoPut operation.
   *
   * <p>Generally, instead of implementing this yourself, you should use {@link AsyncPutListener} or {@link
   * SyncPutListener}.
   */
  public interface PutListener extends StreamListener<PutResult> {

    /**
     * Wait for the stream to finish on the server side. You must call this to be notified of any errors that may have
     * happened during the upload.
     */
    void getResult();

    /**
     * Called when a message from the server is received.
     *
     * @param val The application metadata. This buffer will be reclaimed once onNext returns; you must retain a
     *     reference to use it outside this method.
     */
    @Override
    void onNext(PutResult val);

    /**
     * Check if the call has been cancelled.
     *
     * <p>By default, this always returns false. Implementations should provide an appropriate implementation, as
     * otherwise, a DoPut operation may inadvertently block forever.
     */
    default boolean isCancelled() {
      return false;
    }
  }

  /**
   * Shut down this client.
   */
  public void close() throws InterruptedException {
    channel.shutdown().awaitTermination(5, TimeUnit.SECONDS);
    allocator.close();
  }

  /**
   * Create a builder for a Flight client.
   */
  public static Builder builder() {
    return new Builder();
  }

  /**
   * Create a builder for a Flight client.
   * @param allocator The allocator to use for the client.
   * @param location The location to connect to.
   */
  public static Builder builder(BufferAllocator allocator, Location location) {
    return new Builder(allocator, location);
  }

  /**
   * A builder for Flight clients.
   */
  public static final class Builder {

    private BufferAllocator allocator;
    private Location location;
    private boolean forceTls = false;
    private int maxInboundMessageSize = FlightServer.MAX_GRPC_MESSAGE_SIZE;
    private InputStream trustedCertificates = null;
    private InputStream clientCertificate = null;
    private InputStream clientKey = null;
    private String overrideHostname = null;
    private List<FlightClientMiddleware.Factory> middleware = new ArrayList<>();

    private Builder() {
    }

    private Builder(BufferAllocator allocator, Location location) {
      this.allocator = Preconditions.checkNotNull(allocator);
      this.location = Preconditions.checkNotNull(location);
    }

    /**
     * Force the client to connect over TLS.
     */
    public Builder useTls() {
      this.forceTls = true;
      return this;
    }

    /** Override the hostname checked for TLS. Use with caution in production. */
    public Builder overrideHostname(final String hostname) {
      this.overrideHostname = hostname;
      return this;
    }

    /** Set the maximum inbound message size. */
    public Builder maxInboundMessageSize(int maxSize) {
      Preconditions.checkArgument(maxSize > 0);
      this.maxInboundMessageSize = maxSize;
      return this;
    }

    /** Set the trusted TLS certificates. */
    public Builder trustedCertificates(final InputStream stream) {
      this.trustedCertificates = Preconditions.checkNotNull(stream);
      return this;
    }

    /** Set the trusted TLS certificates. */
    public Builder clientCertificate(final InputStream clientCertificate, final InputStream clientKey) {
      Preconditions.checkNotNull(clientKey);
      this.clientCertificate = Preconditions.checkNotNull(clientCertificate);
      this.clientKey = Preconditions.checkNotNull(clientKey);
      return this;
    }

    public Builder allocator(BufferAllocator allocator) {
      this.allocator = Preconditions.checkNotNull(allocator);
      return this;
    }

    public Builder location(Location location) {
      this.location = Preconditions.checkNotNull(location);
      return this;
    }

    public Builder intercept(FlightClientMiddleware.Factory factory) {
      middleware.add(factory);
      return this;
    }

    /**
     * Create the client from this builder.
     */
    public FlightClient build() {
      final NettyChannelBuilder builder;

      switch (location.getUri().getScheme()) {
        case LocationSchemes.GRPC:
        case LocationSchemes.GRPC_INSECURE:
        case LocationSchemes.GRPC_TLS: {
          builder = NettyChannelBuilder.forAddress(location.toSocketAddress());
          break;
        }
        case LocationSchemes.GRPC_DOMAIN_SOCKET: {
          // The implementation is platform-specific, so we have to find the classes at runtime
          builder = NettyChannelBuilder.forAddress(location.toSocketAddress());
          try {
            try {
              // Linux
              builder.channelType(
                  (Class<? extends ServerChannel>) Class.forName("io.netty.channel.epoll.EpollDomainSocketChannel"));
              final EventLoopGroup elg = (EventLoopGroup) Class.forName("io.netty.channel.epoll.EpollEventLoopGroup")
                  .newInstance();
              builder.eventLoopGroup(elg);
            } catch (ClassNotFoundException e) {
              // BSD
              builder.channelType(
                  (Class<? extends ServerChannel>) Class.forName("io.netty.channel.kqueue.KQueueDomainSocketChannel"));
              final EventLoopGroup elg = (EventLoopGroup) Class.forName("io.netty.channel.kqueue.KQueueEventLoopGroup")
                  .newInstance();
              builder.eventLoopGroup(elg);
            }
          } catch (ClassNotFoundException | InstantiationException | IllegalAccessException e) {
            throw new UnsupportedOperationException(
                "Could not find suitable Netty native transport implementation for domain socket address.");
          }
          break;
        }
        default:
          throw new IllegalArgumentException("Scheme is not supported: " + location.getUri().getScheme());
      }

      if (this.forceTls || LocationSchemes.GRPC_TLS.equals(location.getUri().getScheme())) {
        builder.useTransportSecurity();

        if (this.trustedCertificates != null || this.clientCertificate != null || this.clientKey != null) {
          final SslContextBuilder sslContextBuilder = GrpcSslContexts.forClient();
          if (this.trustedCertificates != null) {
            sslContextBuilder.trustManager(this.trustedCertificates);
          }
          if (this.clientCertificate != null && this.clientKey != null) {
            sslContextBuilder.keyManager(this.clientCertificate, this.clientKey);
          }
          try {
            builder.sslContext(sslContextBuilder.build());
          } catch (SSLException e) {
            throw new RuntimeException(e);
          }
        }

        if (this.overrideHostname != null) {
          builder.overrideAuthority(this.overrideHostname);
        }
      } else {
        builder.usePlaintext();
      }

      builder
          .maxTraceEvents(MAX_CHANNEL_TRACE_EVENTS)
          .maxInboundMessageSize(maxInboundMessageSize);
      return new FlightClient(allocator, builder.build(), middleware);
    }
  }
}
