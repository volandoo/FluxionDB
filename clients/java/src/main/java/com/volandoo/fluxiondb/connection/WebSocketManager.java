package com.volandoo.fluxiondb.connection;

import com.volandoo.fluxiondb.exceptions.AuthenticationException;
import com.volandoo.fluxiondb.exceptions.ConnectionException;
import com.volandoo.fluxiondb.exceptions.TimeoutException;
import com.volandoo.fluxiondb.json.JsonBuilder;
import com.volandoo.fluxiondb.json.JsonParser;
import com.volandoo.fluxiondb.protocol.MessageTypes;
import com.volandoo.fluxiondb.protocol.RequestIdGenerator;

import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.WebSocket;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Manages WebSocket connection lifecycle, message routing, and automatic reconnection.
 */
public class WebSocketManager {

    private final HttpClient httpClient;
    private final String baseUrl;
    private final String apiKey;
    private final AtomicReference<String> connectionName;
    private final long requestTimeoutMs;
    private final ReconnectionStrategy reconnectionStrategy;

    private final AtomicReference<WebSocket> webSocket = new AtomicReference<>();
    private final ConcurrentHashMap<String, CompletableFuture<String>> inflightRequests = new ConcurrentHashMap<>();
    private final AtomicBoolean isConnecting = new AtomicBoolean(false);
    private final AtomicBoolean shouldReconnect = new AtomicBoolean(true);
    private final AtomicInteger reconnectAttempts = new AtomicInteger(0);
    private final AtomicReference<CompletableFuture<Void>> readyFuture = new AtomicReference<>();

    private final ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1, r -> {
        Thread t = new Thread(r, "fluxiondb-timeout-scheduler");
        t.setDaemon(true);
        return t;
    });

    private final StringBuilder messageBuffer = new StringBuilder();

    public WebSocketManager(String url, String apiKey, String connectionName,
                            long requestTimeoutMs, ReconnectionStrategy reconnectionStrategy) {
        this.baseUrl = url;
        this.apiKey = apiKey;
        this.connectionName = new AtomicReference<>(connectionName);
        this.requestTimeoutMs = requestTimeoutMs;
        this.reconnectionStrategy = reconnectionStrategy;
        this.httpClient = HttpClient.newHttpClient();
    }

    /**
     * Establishes WebSocket connection and waits for authentication.
     */
    public CompletableFuture<Void> connect() {
        // If already connected, return immediately
        WebSocket ws = webSocket.get();
        if (ws != null && !ws.isOutputClosed()) {
            return CompletableFuture.completedFuture(null);
        }

        // If already connecting, return existing future
        if (!isConnecting.compareAndSet(false, true)) {
            CompletableFuture<Void> existing = readyFuture.get();
            return existing != null ? existing : CompletableFuture.completedFuture(null);
        }

        CompletableFuture<Void> connectionFuture = new CompletableFuture<>();
        readyFuture.set(connectionFuture);

        try {
            String wsUrl = buildAuthenticatedUrl();
            URI uri = URI.create(wsUrl);

            httpClient.newWebSocketBuilder()
                    .buildAsync(uri, new FluxionDBWebSocketListener())
                    .thenAccept(newWs -> {
                        webSocket.set(newWs);
                        // Don't complete connection here - wait for "ready" message
                    })
                    .exceptionally(ex -> {
                        isConnecting.set(false);
                        connectionFuture.completeExceptionally(
                                new ConnectionException("Failed to establish WebSocket connection", ex));
                        return null;
                    });

            // Timeout for initial connection
            scheduler.schedule(() -> {
                if (!connectionFuture.isDone()) {
                    isConnecting.set(false);
                    connectionFuture.completeExceptionally(
                            new TimeoutException("Connection timeout - no ready message received"));
                }
            }, 10, TimeUnit.SECONDS);

        } catch (Exception e) {
            isConnecting.set(false);
            connectionFuture.completeExceptionally(new ConnectionException("Failed to create WebSocket", e));
        }

        return connectionFuture;
    }

    /**
     * Sends a message and returns a CompletableFuture for the response.
     */
    public CompletableFuture<String> send(String type, String data) {
        return connect().thenCompose(v -> {
            WebSocket ws = webSocket.get();
            if (ws == null || ws.isOutputClosed()) {
                return CompletableFuture.failedFuture(
                        new ConnectionException("WebSocket not connected"));
            }

            String messageId = RequestIdGenerator.generate();
            CompletableFuture<String> responseFuture = new CompletableFuture<>();

            inflightRequests.put(messageId, responseFuture);

            String message = new JsonBuilder()
                    .add("id", messageId)
                    .add("type", type)
                    .add("data", data)
                    .build();

            ws.sendText(message, true)
                    .exceptionally(ex -> {
                        inflightRequests.remove(messageId);
                        responseFuture.completeExceptionally(
                                new ConnectionException("Failed to send message", ex));
                        return null;
                    });

            // Schedule timeout
            scheduler.schedule(() -> {
                CompletableFuture<String> removed = inflightRequests.remove(messageId);
                if (removed != null && !removed.isDone()) {
                    removed.completeExceptionally(new TimeoutException("Request timeout after " + requestTimeoutMs + "ms"));
                }
            }, requestTimeoutMs, TimeUnit.MILLISECONDS);

            return responseFuture;
        });
    }

    /**
     * Closes the WebSocket connection and prevents reconnection.
     */
    public CompletableFuture<Void> close() {
        shouldReconnect.set(false);

        WebSocket ws = webSocket.get();
        if (ws != null) {
            return ws.sendClose(WebSocket.NORMAL_CLOSURE, "Client closing")
                    .thenRun(() -> {
                        webSocket.set(null);
                        cleanupInflightRequests();
                    });
        }

        return CompletableFuture.completedFuture(null);
    }

    public void setConnectionName(String name) {
        this.connectionName.set(name);
    }

    public void shutdown() {
        shouldReconnect.set(false);
        scheduler.shutdown();
        close().join();
    }

    private String buildAuthenticatedUrl() {
        try {
            StringBuilder url = new StringBuilder(baseUrl);
            url.append("?api-key=").append(URLEncoder.encode(apiKey, StandardCharsets.UTF_8.name()));

            String name = connectionName.get();
            if (name != null && !name.isEmpty()) {
                url.append("&name=").append(URLEncoder.encode(name, StandardCharsets.UTF_8.name()));
            }

            return url.toString();
        } catch (Exception e) {
            throw new IllegalArgumentException("Failed to build URL", e);
        }
    }

    private CompletableFuture<Void> reconnect() {
        if (!shouldReconnect.get()) {
            return CompletableFuture.completedFuture(null);
        }

        int attempt = reconnectAttempts.incrementAndGet();

        if (!reconnectionStrategy.shouldRetry(attempt)) {
            cleanupInflightRequests();
            return CompletableFuture.failedFuture(
                    new ConnectionException("Max reconnection attempts (" + reconnectionStrategy.getMaxAttempts() + ") exceeded"));
        }

        long delayMs = reconnectionStrategy.getDelayMs(attempt);

        return CompletableFuture.runAsync(() -> {
            try {
                Thread.sleep(delayMs);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }).thenCompose(v -> {
            isConnecting.set(false);
            return connect();
        });
    }

    private void cleanupInflightRequests() {
        ConnectionException error = new ConnectionException("Connection closed");
        inflightRequests.forEach((id, future) -> {
            if (!future.isDone()) {
                future.completeExceptionally(error);
            }
        });
        inflightRequests.clear();
    }

    /**
     * WebSocket.Listener implementation for handling WebSocket events.
     */
    private class FluxionDBWebSocketListener implements WebSocket.Listener {

        @Override
        public void onOpen(WebSocket webSocket) {
            webSocket.request(1);
        }

        @Override
        public CompletionStage<?> onText(WebSocket webSocket, CharSequence data, boolean last) {
            messageBuffer.append(data);

            if (last) {
                String completeMessage = messageBuffer.toString();
                messageBuffer.setLength(0);

                try {
                    handleMessage(completeMessage);
                } catch (Exception e) {
                    System.err.println("Error handling message: " + e.getMessage());
                }
            }

            webSocket.request(1);
            return CompletableFuture.completedFuture(null);
        }

        @Override
        public CompletionStage<?> onClose(WebSocket webSocket, int statusCode, String reason) {
            WebSocketManager.this.webSocket.set(null);

            // If authenticated, trigger reconnection
            CompletableFuture<Void> ready = readyFuture.get();
            if (ready != null && ready.isDone() && !ready.isCompletedExceptionally()) {
                if (shouldReconnect.get()) {
                    reconnect();
                }
            }

            return CompletableFuture.completedFuture(null);
        }

        @Override
        public void onError(WebSocket webSocket, Throwable error) {
            System.err.println("WebSocket error: " + error.getMessage());

            CompletableFuture<Void> ready = readyFuture.get();
            if (ready != null && !ready.isDone()) {
                ready.completeExceptionally(new ConnectionException("WebSocket error during connection", error));
                isConnecting.set(false);
            } else if (shouldReconnect.get()) {
                reconnect();
            }
        }

        private void handleMessage(String message) {
            Map<String, Object> parsed = JsonParser.parseObject(message);
            String type = JsonParser.getString(parsed, "type");

            // Handle "ready" message (authentication successful)
            if (MessageTypes.READY.equals(type)) {
                CompletableFuture<Void> ready = readyFuture.get();
                if (ready != null) {
                    ready.complete(null);
                    reconnectAttempts.set(0); // Reset reconnection counter
                }
                isConnecting.set(false);
                return;
            }

            // Route response to inflight request
            String id = JsonParser.getString(parsed, "id");
            if (id != null) {
                CompletableFuture<String> responseFuture = inflightRequests.remove(id);
                if (responseFuture != null) {
                    // Check for error in response
                    String error = JsonParser.getString(parsed, "error");
                    if (error != null) {
                        responseFuture.completeExceptionally(
                                new com.volandoo.fluxiondb.exceptions.FluxionDBException("Server error: " + error));
                    } else {
                        responseFuture.complete(message);
                    }
                }
            }
        }
    }
}
