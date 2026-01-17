# FluxionDB Java Client

Official Java client for FluxionDB, an in-memory time series database with optional persistence.

## Features

- ✅ **Zero Dependencies**: Uses only JDK 11+ standard library
- ✅ **Async API**: CompletableFuture-based for non-blocking operations
- ✅ **Automatic Reconnection**: Exponential backoff retry strategy
- ✅ **Thread-Safe**: Designed for concurrent use
- ✅ **Type-Safe**: Immutable POJOs with builder patterns
- ✅ **WebSocket Protocol**: Efficient bidirectional communication

## Requirements

- **Java 11 or higher**
- No third-party dependencies

## Quick Start

### Build from Source

```bash
cd clients/java
./build.sh
```

This creates `fluxiondb-client.jar` in the current directory.

### Basic Usage

```java
import com.volandoo.fluxiondb.*;
import com.volandoo.fluxiondb.model.requests.*;
import com.volandoo.fluxiondb.model.responses.*;

// Create client
FluxionDBClient client = new FluxionDBClientBuilder()
    .url("ws://localhost:8080")
    .apiKey("YOUR_SECRET_KEY")
    .connectionName("java-client")
    .build();

// Connect
client.connect().get();

// Insert a record
long now = System.currentTimeMillis() / 1000;
InsertMessageRequest record = new InsertMessageRequest(
    now,
    "device-1",
    "{\"temperature\": 22.5}",
    "sensors"
);

client.insertSingleRecord(record).get();

// Fetch collections
List<String> collections = client.fetchCollections().get();
System.out.println(collections);

// Close connection (async)
client.closeAsync().get();

// Or use try-with-resources (synchronous)
try (FluxionDBClient client = ...) {
    // Use client
} // Auto-closes
```

### Async Style

```java
client.fetchCollections()
    .thenAccept(collections -> System.out.println(collections))
    .exceptionally(ex -> {
        System.err.println("Error: " + ex.getMessage());
        return null;
    });
```

## API Overview

### Connection Management

```java
client.connect()                        // CompletableFuture<Void>
client.closeAsync()                     // CompletableFuture<Void>
client.close()                          // void (AutoCloseable, blocking)
client.setConnectionName(String name)   // void
```

### Time Series Operations

```java
client.insertSingleRecord(InsertMessageRequest)
client.insertMultipleRecords(List<InsertMessageRequest>)
client.fetchLatestRecords(FetchLatestRecordsParams)
client.fetchDocument(FetchRecordsParams)
client.deleteDocument(DeleteDocumentParams)
client.deleteRecord(DeleteRecord)
client.deleteMultipleRecords(List<DeleteRecord>)
client.deleteRecordsRange(DeleteRecordsRange)
```

### Collection Operations

```java
client.fetchCollections()                           // CompletableFuture<List<String>>
client.deleteCollection(DeleteCollectionParams)     // CompletableFuture<Void>
```

### Key-Value Operations

```java
client.setValue(SetValueParams)                     // CompletableFuture<Void>
client.getValue(GetValueParams)                     // CompletableFuture<String>
client.getValues(GetValuesParams)                   // CompletableFuture<Map<String, String>>
client.getKeys(CollectionParam)                     // CompletableFuture<List<String>>
client.deleteValue(DeleteValueParams)               // CompletableFuture<Void>
```

### Management Operations

```java
client.getConnections()                             // CompletableFuture<List<ConnectionInfo>>
client.addApiKey(String key, ApiKeyScope scope)     // CompletableFuture<String>
client.removeApiKey(String key)                     // CompletableFuture<String>
client.listApiKeys()                                // CompletableFuture<List<ApiKeyInfo>>
```

## Configuration

```java
FluxionDBClient client = new FluxionDBClientBuilder()
    .url("ws://localhost:8080")              // Required: WebSocket URL
    .apiKey("YOUR_SECRET_KEY")               // Required: API key
    .connectionName("java-client")           // Optional: Connection identifier
    .maxReconnectAttempts(5)                 // Optional: Max reconnection attempts (default: 5)
    .reconnectInterval(5000)                 // Optional: Base reconnect interval in ms (default: 5000)
    .requestTimeout(30000)                   // Optional: Request timeout in ms (default: 30000)
    .build();
```

## Regex Pattern Support

Document IDs and key-value keys support regex patterns:

```java
// Fetch records for documents matching regex
FetchLatestRecordsParams params = FetchLatestRecordsParams.builder()
    .col("sensors")
    .ts(now)
    .doc("/device-[0-9]+/")  // Regex pattern
    .build();

Map<String, RecordResponse> records = client.fetchLatestRecords(params).get();

// Get key-value pairs matching pattern
GetValuesParams kvParams = new GetValuesParams("config", "/env\\..*/");
Map<String, String> envVars = client.getValues(kvParams).get();
```

## Error Handling

```java
try {
    client.connect().get();
    // Operations...
} catch (ConnectionException e) {
    // Connection failed
} catch (TimeoutException e) {
    // Request timed out
} catch (AuthenticationException e) {
    // Authentication failed
} catch (FluxionDBException e) {
    // General FluxionDB error
}
```

## Thread Safety

The client is thread-safe and designed for concurrent use:

```java
FluxionDBClient client = /* ... */;

// Safe to use from multiple threads
ExecutorService executor = Executors.newFixedThreadPool(10);

for (int i = 0; i < 100; i++) {
    final int deviceId = i;
    executor.submit(() -> {
        client.insertSingleRecord(new InsertMessageRequest(
            System.currentTimeMillis() / 1000,
            "device-" + deviceId,
            "{\"value\": " + deviceId + "}",
            "sensors"
        )).join();
    });
}
```

## Examples

See `examples/BasicUsageExample.java` for a complete working example.

## Building

### Command Line

```bash
./build.sh
```

### Maven (optional)

```xml
<!-- No dependencies required -->
<build>
    <plugins>
        <plugin>
            <groupId>org.apache.maven.plugins</groupId>
            <artifactId>maven-compiler-plugin</artifactId>
            <version>3.8.1</version>
            <configuration>
                <source>11</source>
                <target>11</target>
            </configuration>
        </plugin>
    </plugins>
</build>
```

### Gradle (optional)

```gradle
plugins {
    id 'java-library'
}

java {
    sourceCompatibility = JavaVersion.VERSION_11
    targetCompatibility = JavaVersion.VERSION_11
}
```

## Architecture

The client is organized into layers:

- **FluxionDBClient**: Main entry point, delegates to operation classes
- **WebSocketManager**: Manages WebSocket lifecycle, reconnection, and message routing
- **Operations**: TimeSeriesOperations, CollectionOperations, KeyValueOperations, ManagementOperations
- **JSON**: Zero-dependency JSON builder and parser
- **Models**: Immutable POJOs for requests and responses

## License

Apache-2.0

## See Also

- [DOCUMENTATION.md](DOCUMENTATION.md) - Detailed API reference
- Main repository: [FluxionDB](../../README.md)
- Node.js client: [clients/node/](../node/)
- Go client: [clients/go/](../go/)
- Python client: [clients/python/](../python/)
