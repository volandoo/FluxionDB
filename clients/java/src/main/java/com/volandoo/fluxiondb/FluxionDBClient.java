package com.volandoo.fluxiondb;

import com.volandoo.fluxiondb.connection.ReconnectionStrategy;
import com.volandoo.fluxiondb.connection.WebSocketManager;
import com.volandoo.fluxiondb.model.enums.ApiKeyScope;
import com.volandoo.fluxiondb.model.requests.*;
import com.volandoo.fluxiondb.model.responses.ApiKeyInfo;
import com.volandoo.fluxiondb.model.responses.ConnectionInfo;
import com.volandoo.fluxiondb.model.responses.RecordResponse;
import com.volandoo.fluxiondb.operations.CollectionOperations;
import com.volandoo.fluxiondb.operations.KeyValueOperations;
import com.volandoo.fluxiondb.operations.ManagementOperations;
import com.volandoo.fluxiondb.operations.TimeSeriesOperations;

import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;

/**
 * Main client for FluxionDB.
 * Provides async operations for time series data, collections, key-value storage, and management.
 * <p>
 * Example usage:
 * <pre>
 * FluxionDBClient client = new FluxionDBClientBuilder()
 *     .url("ws://localhost:8080")
 *     .apiKey("YOUR_SECRET_KEY")
 *     .connectionName("java-client")
 *     .build();
 *
 * client.connect().get();
 * client.insertSingleRecord(record).get();
 * List&lt;String&gt; collections = client.fetchCollections().get();
 * client.close().get();
 * </pre>
 */
public class FluxionDBClient implements AutoCloseable {

    private final WebSocketManager wsManager;
    private final TimeSeriesOperations timeSeries;
    private final CollectionOperations collections;
    private final KeyValueOperations keyValue;
    private final ManagementOperations management;

    /**
     * Creates a new FluxionDBClient. Use FluxionDBClientBuilder instead of calling this directly.
     */
    protected FluxionDBClient(String url, String apiKey, String connectionName,
                              long requestTimeoutMs, ReconnectionStrategy reconnectionStrategy) {
        this.wsManager = new WebSocketManager(url, apiKey, connectionName,
                requestTimeoutMs, reconnectionStrategy);
        this.timeSeries = new TimeSeriesOperations(wsManager);
        this.collections = new CollectionOperations(wsManager);
        this.keyValue = new KeyValueOperations(wsManager);
        this.management = new ManagementOperations(wsManager);
    }

    // ==================== Connection Management ====================

    /**
     * Establishes WebSocket connection and authenticates with the server.
     *
     * @return CompletableFuture that completes when connection is established and authenticated
     */
    public CompletableFuture<Void> connect() {
        return wsManager.connect();
    }

    /**
     * Closes the WebSocket connection asynchronously and prevents automatic reconnection.
     *
     * @return CompletableFuture that completes when connection is closed
     */
    public CompletableFuture<Void> closeAsync() {
        return wsManager.close();
    }

    /**
     * Sets an optional connection name for identification.
     *
     * @param name the connection name
     */
    public void setConnectionName(String name) {
        wsManager.setConnectionName(name);
    }

    /**
     * Closes the WebSocket connection synchronously (AutoCloseable implementation).
     * Blocks until connection is fully closed.
     */
    @Override
    public void close() throws Exception {
        wsManager.shutdown();
    }

    // ==================== Time Series Operations ====================

    /**
     * Inserts a single time series record.
     *
     * @param request the insert request
     * @return CompletableFuture that completes when insert is acknowledged
     */
    public CompletableFuture<Void> insertSingleRecord(InsertMessageRequest request) {
        return timeSeries.insertSingleRecord(request);
    }

    /**
     * Inserts multiple time series records in a single request.
     *
     * @param requests list of insert requests
     * @return CompletableFuture that completes when inserts are acknowledged
     */
    public CompletableFuture<Void> insertMultipleRecords(List<InsertMessageRequest> requests) {
        return timeSeries.insertMultipleRecords(requests);
    }

    /**
     * Fetches the latest record per document.
     *
     * @param params fetch parameters (supports regex patterns)
     * @return CompletableFuture with map of document IDs to latest records
     */
    public CompletableFuture<Map<String, RecordResponse>> fetchLatestRecords(FetchLatestRecordsParams params) {
        return timeSeries.fetchLatestRecords(params);
    }

    /**
     * Fetches all records for a document within a time range.
     *
     * @param params fetch parameters
     * @return CompletableFuture with list of records
     */
    public CompletableFuture<List<RecordResponse>> fetchDocument(FetchRecordsParams params) {
        return timeSeries.fetchDocument(params);
    }

    /**
     * Deletes all records for a document.
     *
     * @param params delete parameters
     * @return CompletableFuture that completes when delete is acknowledged
     */
    public CompletableFuture<Void> deleteDocument(DeleteDocumentParams params) {
        return timeSeries.deleteDocument(params);
    }

    /**
     * Deletes a single record.
     *
     * @param params delete parameters
     * @return CompletableFuture that completes when delete is acknowledged
     */
    public CompletableFuture<Void> deleteRecord(DeleteRecord params) {
        return timeSeries.deleteRecord(params);
    }

    /**
     * Deletes multiple records.
     *
     * @param records list of records to delete
     * @return CompletableFuture that completes when deletes are acknowledged
     */
    public CompletableFuture<Void> deleteMultipleRecords(List<DeleteRecord> records) {
        return timeSeries.deleteMultipleRecords(records);
    }

    /**
     * Deletes records within a timestamp range.
     *
     * @param params delete range parameters
     * @return CompletableFuture that completes when delete is acknowledged
     */
    public CompletableFuture<Void> deleteRecordsRange(DeleteRecordsRange params) {
        return timeSeries.deleteRecordsRange(params);
    }

    // ==================== Collection Operations ====================

    /**
     * Fetches list of all collections.
     *
     * @return CompletableFuture with list of collection names
     */
    public CompletableFuture<List<String>> fetchCollections() {
        return collections.fetchCollections();
    }

    /**
     * Deletes an entire collection and all its data.
     *
     * @param params delete parameters
     * @return CompletableFuture that completes when delete is acknowledged
     */
    public CompletableFuture<Void> deleteCollection(DeleteCollectionParams params) {
        return collections.deleteCollection(params);
    }

    // ==================== Key-Value Operations ====================

    /**
     * Sets a key-value pair in a collection.
     *
     * @param params set value parameters
     * @return CompletableFuture that completes when set is acknowledged
     */
    public CompletableFuture<Void> setValue(SetValueParams params) {
        return keyValue.setValue(params);
    }

    /**
     * Gets a value by key from a collection.
     *
     * @param params get value parameters
     * @return CompletableFuture with the value
     */
    public CompletableFuture<String> getValue(GetValueParams params) {
        return keyValue.getValue(params);
    }

    /**
     * Gets multiple values, optionally filtered by regex pattern.
     *
     * @param params get values parameters (key can be null or regex pattern)
     * @return CompletableFuture with map of key-value pairs
     */
    public CompletableFuture<Map<String, String>> getValues(GetValuesParams params) {
        return keyValue.getValues(params);
    }

    /**
     * Gets all keys in a collection.
     *
     * @param params collection parameters
     * @return CompletableFuture with list of keys
     */
    public CompletableFuture<List<String>> getKeys(CollectionParam params) {
        return keyValue.getKeys(params);
    }

    /**
     * Deletes a key-value pair.
     *
     * @param params delete value parameters
     * @return CompletableFuture that completes when delete is acknowledged
     */
    public CompletableFuture<Void> deleteValue(DeleteValueParams params) {
        return keyValue.deleteValue(params);
    }

    // ==================== Management Operations ====================

    /**
     * Gets list of active connections.
     *
     * @return CompletableFuture with list of connection info
     */
    public CompletableFuture<List<ConnectionInfo>> getConnections() {
        return management.getConnections();
    }

    /**
     * Adds a new scoped API key.
     *
     * @param key   the new API key
     * @param scope the permission scope
     * @return CompletableFuture with operation result
     */
    public CompletableFuture<String> addApiKey(String key, ApiKeyScope scope) {
        return management.addApiKey(key, scope);
    }

    /**
     * Removes an API key.
     *
     * @param key the API key to remove
     * @return CompletableFuture with operation result
     */
    public CompletableFuture<String> removeApiKey(String key) {
        return management.removeApiKey(key);
    }

    /**
     * Lists all API keys.
     *
     * @return CompletableFuture with list of API key info
     */
    public CompletableFuture<List<ApiKeyInfo>> listApiKeys() {
        return management.listApiKeys();
    }
}
