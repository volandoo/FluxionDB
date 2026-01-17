package com.volandoo.fluxiondb;

import com.volandoo.fluxiondb.connection.ReconnectionStrategy;

/**
 * Builder for creating FluxionDBClient instances with fluent configuration.
 */
public class FluxionDBClientBuilder {

    private String url;
    private String apiKey;
    private String connectionName;
    private int maxReconnectAttempts = 5;
    private long reconnectIntervalMs = 5000;
    private long requestTimeoutMs = 30000;

    public FluxionDBClientBuilder() {
    }

    /**
     * Sets the FluxionDB WebSocket URL.
     *
     * @param url WebSocket URL (e.g., "ws://localhost:8080")
     * @return this builder
     */
    public FluxionDBClientBuilder url(String url) {
        this.url = url;
        return this;
    }

    /**
     * Sets the API key for authentication.
     *
     * @param apiKey the API key
     * @return this builder
     */
    public FluxionDBClientBuilder apiKey(String apiKey) {
        this.apiKey = apiKey;
        return this;
    }

    /**
     * Sets an optional connection name for identification.
     *
     * @param connectionName the connection name
     * @return this builder
     */
    public FluxionDBClientBuilder connectionName(String connectionName) {
        this.connectionName = connectionName;
        return this;
    }

    /**
     * Sets the maximum number of reconnection attempts.
     *
     * @param maxReconnectAttempts max attempts (default: 5)
     * @return this builder
     */
    public FluxionDBClientBuilder maxReconnectAttempts(int maxReconnectAttempts) {
        this.maxReconnectAttempts = maxReconnectAttempts;
        return this;
    }

    /**
     * Sets the base reconnection interval in milliseconds.
     *
     * @param reconnectIntervalMs interval in milliseconds (default: 5000)
     * @return this builder
     */
    public FluxionDBClientBuilder reconnectInterval(long reconnectIntervalMs) {
        this.reconnectIntervalMs = reconnectIntervalMs;
        return this;
    }

    /**
     * Sets the request timeout in milliseconds.
     *
     * @param requestTimeoutMs timeout in milliseconds (default: 30000)
     * @return this builder
     */
    public FluxionDBClientBuilder requestTimeout(long requestTimeoutMs) {
        this.requestTimeoutMs = requestTimeoutMs;
        return this;
    }

    /**
     * Builds the FluxionDBClient instance.
     *
     * @return configured FluxionDBClient
     * @throws IllegalArgumentException if required parameters are missing
     */
    public FluxionDBClient build() {
        if (url == null || url.trim().isEmpty()) {
            throw new IllegalArgumentException("URL is required");
        }
        if (apiKey == null || apiKey.trim().isEmpty()) {
            throw new IllegalArgumentException("API key is required");
        }

        ReconnectionStrategy reconnectionStrategy =
                new ReconnectionStrategy(maxReconnectAttempts, reconnectIntervalMs);

        return new FluxionDBClient(
                url,
                apiKey,
                connectionName,
                requestTimeoutMs,
                reconnectionStrategy
        );
    }
}
