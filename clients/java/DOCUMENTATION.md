# FluxionDB Java Client - API Documentation

Complete API reference for the FluxionDB Java client.

## Table of Contents

- [Client Creation](#client-creation)
- [Connection Management](#connection-management)
- [Time Series Operations](#time-series-operations)
- [Collection Operations](#collection-operations)
- [Key-Value Operations](#key-value-operations)
- [Management Operations](#management-operations)
- [Request Types](#request-types)
- [Response Types](#response-types)
- [Enums](#enums)
- [Exceptions](#exceptions)

## Client Creation

### FluxionDBClientBuilder

Fluent builder for creating `FluxionDBClient` instances.

```java
FluxionDBClient client = new FluxionDBClientBuilder()
    .url(String url)                        // Required: WebSocket URL
    .apiKey(String apiKey)                  // Required: API key for authentication
    .connectionName(String name)            // Optional: Connection identifier
    .maxReconnectAttempts(int attempts)     // Optional: Default 5
    .reconnectInterval(long millis)         // Optional: Default 5000ms
    .requestTimeout(long millis)            // Optional: Default 30000ms
    .build();
```

**Example:**
```java
FluxionDBClient client = new FluxionDBClientBuilder()
    .url("ws://localhost:8080")
    .apiKey("my-secret-key")
    .connectionName("production-consumer")
    .maxReconnectAttempts(10)
    .reconnectInterval(3000)
    .requestTimeout(60000)
    .build();
```

## Connection Management

### connect()

Establishes WebSocket connection and authenticates with the server.

```java
CompletableFuture<Void> connect()
```

**Returns:** `CompletableFuture<Void>` that completes when connected and authenticated

**Throws:**
- `ConnectionException` - Connection failed
- `TimeoutException` - Connection timeout
- `AuthenticationException` - Authentication failed

**Example:**
```java
client.connect()
    .thenRun(() -> System.out.println("Connected!"))
    .exceptionally(ex -> {
        System.err.println("Failed: " + ex.getMessage());
        return null;
    });

// Or blocking style:
client.connect().get();
```

### closeAsync()

Closes the WebSocket connection asynchronously and prevents automatic reconnection.

```java
CompletableFuture<Void> closeAsync()
```

**Example:**
```java
client.closeAsync().get();
```

### close() (AutoCloseable)

Closes the WebSocket connection synchronously. Implements `AutoCloseable` for try-with-resources.

```java
void close() throws Exception
```

**Example:**
```java
try (FluxionDBClient client = createClient()) {
    client.connect().get();
    // Use client
} // Auto-closes synchronously
```

### setConnectionName()

Sets an optional connection name for identification in connection listings.

```java
void setConnectionName(String name)
```

**Example:**
```java
client.setConnectionName("analytics-processor-1");
```

## Time Series Operations

### insertSingleRecord()

Inserts a single time series record.

```java
CompletableFuture<Void> insertSingleRecord(InsertMessageRequest request)
```

**Parameters:**
- `request` - Insert request with ts, doc, data, col

**Example:**
```java
long now = System.currentTimeMillis() / 1000;
InsertMessageRequest record = new InsertMessageRequest(
    now,                                    // timestamp (Unix epoch seconds)
    "device-123",                           // document ID
    "{\"temp\": 22.5, \"unit\": \"C\"}",   // JSON data as string
    "sensors"                               // collection name
);

client.insertSingleRecord(record).get();
```

### insertMultipleRecords()

Inserts multiple time series records in a single request.

```java
CompletableFuture<Void> insertMultipleRecords(List<InsertMessageRequest> requests)
```

**Example:**
```java
List<InsertMessageRequest> records = Arrays.asList(
    new InsertMessageRequest(now, "device-1", "{\"value\": 10}", "sensors"),
    new InsertMessageRequest(now, "device-2", "{\"value\": 20}", "sensors"),
    new InsertMessageRequest(now, "device-3", "{\"value\": 30}", "sensors")
);

client.insertMultipleRecords(records).get();
```

### fetchLatestRecords()

Fetches the latest record per document.

```java
CompletableFuture<Map<String, RecordResponse>> fetchLatestRecords(FetchLatestRecordsParams params)
```

**Parameters:**
- `params` - Fetch parameters (supports regex patterns in `doc` field)

**Returns:** Map of document IDs to latest records

**Example:**
```java
FetchLatestRecordsParams params = FetchLatestRecordsParams.builder()
    .col("sensors")
    .ts(now)
    .doc("/device-[0-9]+/")  // Regex pattern (optional)
    .from(now - 3600)        // Optional: filter by timestamp
    .build();

Map<String, RecordResponse> latest = client.fetchLatestRecords(params).get();

for (Map.Entry<String, RecordResponse> entry : latest.entrySet()) {
    System.out.println(entry.getKey() + ": " + entry.getValue().getData());
}
```

### fetchDocument()

Fetches all records for a document within a time range.

```java
CompletableFuture<List<RecordResponse>> fetchDocument(FetchRecordsParams params)
```

**Parameters:**
- `params` - Fetch parameters with time range

**Example:**
```java
FetchRecordsParams params = FetchRecordsParams.builder()
    .col("sensors")
    .doc("device-123")
    .from(now - 86400)     // Last 24 hours
    .to(now)
    .limit(100)            // Optional: max records
    .reverse(true)         // Optional: newest first
    .build();

List<RecordResponse> history = client.fetchDocument(params).get();
```

### deleteDocument()

Deletes all records for a document.

```java
CompletableFuture<Void> deleteDocument(DeleteDocumentParams params)
```

**Example:**
```java
DeleteDocumentParams params = new DeleteDocumentParams("sensors", "device-123");
client.deleteDocument(params).get();
```

### deleteRecord()

Deletes a single record by timestamp.

```java
CompletableFuture<Void> deleteRecord(DeleteRecord params)
```

**Example:**
```java
DeleteRecord params = new DeleteRecord("sensors", "device-123", timestamp);
client.deleteRecord(params).get();
```

### deleteMultipleRecords()

Deletes multiple specific records.

```java
CompletableFuture<Void> deleteMultipleRecords(List<DeleteRecord> records)
```

**Example:**
```java
List<DeleteRecord> toDelete = Arrays.asList(
    new DeleteRecord("sensors", "device-1", ts1),
    new DeleteRecord("sensors", "device-2", ts2)
);

client.deleteMultipleRecords(toDelete).get();
```

### deleteRecordsRange()

Deletes all records within a timestamp range.

```java
CompletableFuture<Void> deleteRecordsRange(DeleteRecordsRange params)
```

**Example:**
```java
DeleteRecordsRange params = new DeleteRecordsRange(
    "sensors",          // collection
    "device-123",       // document
    now - 86400,        // from timestamp
    now - 3600          // to timestamp
);

client.deleteRecordsRange(params).get();
```

## Collection Operations

### fetchCollections()

Fetches list of all collections.

```java
CompletableFuture<List<String>> fetchCollections()
```

**Example:**
```java
List<String> collections = client.fetchCollections().get();
System.out.println("Collections: " + collections);
```

### deleteCollection()

Deletes an entire collection and all its data.

```java
CompletableFuture<Void> deleteCollection(DeleteCollectionParams params)
```

**Example:**
```java
DeleteCollectionParams params = new DeleteCollectionParams("old_sensors");
client.deleteCollection(params).get();
```

## Key-Value Operations

### setValue()

Sets a key-value pair in a collection.

```java
CompletableFuture<Void> setValue(SetValueParams params)
```

**Example:**
```java
SetValueParams params = new SetValueParams(
    "config",           // collection
    "app.version",      // key
    "1.2.3"            // value
);

client.setValue(params).get();
```

### getValue()

Gets a value by key.

```java
CompletableFuture<String> getValue(GetValueParams params)
```

**Example:**
```java
GetValueParams params = new GetValueParams("config", "app.version");
String version = client.getValue(params).get();
```

### getValues()

Gets multiple values, optionally filtered by regex pattern.

```java
CompletableFuture<Map<String, String>> getValues(GetValuesParams params)
```

**Example:**
```java
// Get all values
GetValuesParams params1 = new GetValuesParams("config");
Map<String, String> all = client.getValues(params1).get();

// Get values matching regex
GetValuesParams params2 = new GetValuesParams("config", "/env\\..*/");
Map<String, String> envVars = client.getValues(params2).get();
```

### getKeys()

Gets all keys in a collection.

```java
CompletableFuture<List<String>> getKeys(CollectionParam params)
```

**Example:**
```java
CollectionParam params = new CollectionParam("config");
List<String> keys = client.getKeys(params).get();
```

### deleteValue()

Deletes a key-value pair.

```java
CompletableFuture<Void> deleteValue(DeleteValueParams params)
```

**Example:**
```java
DeleteValueParams params = new DeleteValueParams("config", "old.setting");
client.deleteValue(params).get();
```

## Management Operations

### getConnections()

Gets list of active client connections.

```java
CompletableFuture<List<ConnectionInfo>> getConnections()
```

**Example:**
```java
List<ConnectionInfo> connections = client.getConnections().get();

for (ConnectionInfo conn : connections) {
    System.out.printf("IP: %s, Since: %d, Self: %b, Name: %s%n",
        conn.getIp(), conn.getSince(), conn.isSelf(), conn.getName());
}
```

### addApiKey()

Adds a new scoped API key (requires master key).

```java
CompletableFuture<String> addApiKey(String key, ApiKeyScope scope)
```

**Example:**
```java
client.addApiKey("analytics-readonly", ApiKeyScope.READONLY).get();
client.addApiKey("sensor-writer", ApiKeyScope.READ_WRITE).get();
client.addApiKey("admin-key", ApiKeyScope.READ_WRITE_DELETE).get();
```

### removeApiKey()

Removes an API key.

```java
CompletableFuture<String> removeApiKey(String key)
```

**Example:**
```java
client.removeApiKey("old-key").get();
```

### listApiKeys()

Lists all API keys with their scopes.

```java
CompletableFuture<List<ApiKeyInfo>> listApiKeys()
```

**Example:**
```java
List<ApiKeyInfo> keys = client.listApiKeys().get();

for (ApiKeyInfo key : keys) {
    System.out.printf("Key: %s, Scope: %s, Deletable: %b%n",
        key.getKey(), key.getScope(), key.isDeletable());
}
```

## Request Types

### InsertMessageRequest

```java
public InsertMessageRequest(long ts, String doc, String data, String col)
```

- `ts` - Unix timestamp (seconds)
- `doc` - Document ID
- `data` - JSON data as string
- `col` - Collection name

### FetchLatestRecordsParams

```java
FetchLatestRecordsParams.builder()
    .col(String col)          // Required
    .ts(long ts)              // Required
    .doc(String doc)          // Optional: literal or /regex/
    .from(long from)          // Optional: timestamp filter
    .build()
```

### FetchRecordsParams

```java
FetchRecordsParams.builder()
    .col(String col)          // Required
    .doc(String doc)          // Required
    .from(long from)          // Required
    .to(long to)              // Required
    .limit(int limit)         // Optional
    .reverse(boolean reverse) // Optional
    .build()
```

## Response Types

### RecordResponse

```java
public class RecordResponse {
    public long getTs()        // Timestamp
    public String getData()    // JSON data
}
```

### ConnectionInfo

```java
public class ConnectionInfo {
    public String getIp()      // Client IP address
    public long getSince()     // Connection timestamp
    public boolean isSelf()    // Is this connection
    public String getName()    // Connection name (optional)
}
```

### ApiKeyInfo

```java
public class ApiKeyInfo {
    public String getKey()        // API key
    public String getScope()      // Scope string
    public boolean isDeletable()  // Can be deleted
}
```

## Enums

### ApiKeyScope

```java
public enum ApiKeyScope {
    READONLY,              // Query-only access
    READ_WRITE,            // Read and write access
    READ_WRITE_DELETE      // Full access including delete
}
```

## Exceptions

### FluxionDBException

Base exception for all FluxionDB errors.

```java
public class FluxionDBException extends RuntimeException
```

### ConnectionException

WebSocket connection failed or lost.

```java
public class ConnectionException extends FluxionDBException
```

### TimeoutException

Request timed out waiting for response.

```java
public class TimeoutException extends FluxionDBException
```

### AuthenticationException

Authentication with server failed.

```java
public class AuthenticationException extends FluxionDBException
```

## Best Practices

### Resource Management

Always close the client when done:

```java
try (FluxionDBClient client = createClient()) {
    client.connect().get();
    // Use client...
} // Auto-closes
```

### Error Handling

Handle specific exceptions:

```java
client.connect()
    .exceptionally(ex -> {
        if (ex instanceof TimeoutException) {
            // Retry or alert
        } else if (ex instanceof AuthenticationException) {
            // Check API key
        }
        return null;
    });
```

### Batch Operations

Use batch methods for better performance:

```java
// Good: Single batch request
client.insertMultipleRecords(records).get();

// Avoid: Multiple individual requests
for (InsertMessageRequest record : records) {
    client.insertSingleRecord(record).get();
}
```

### Async Chaining

Leverage CompletableFuture composition:

```java
client.connect()
    .thenCompose(v -> client.fetchCollections())
    .thenAccept(collections -> {
        System.out.println("Collections: " + collections);
    })
    .exceptionally(ex -> {
        System.err.println("Error: " + ex.getMessage());
        return null;
    });
```
