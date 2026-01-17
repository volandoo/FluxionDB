# FluxionDB

FluxionDB is an in-memory time series database exposed over WebSockets. The server keeps hot data in RAM, can optionally persist to disk, and speaks a compact protocol that is shared by the official Node.js, Go, Python, and Java client SDKs shipped in this repository.

## Highlights

-   Sub-millisecond inserts and queries backed by an in-memory collection store
-   Compact WebSocket protocol with query-parameter auth and tokenised message types (`ins`, `qry`, …)
-   Optional persistence when a data folder is supplied
-   First-party clients:
    -   Node.js SDK (`clients/node`) published as `@volandoo/fluxiondb-client`
    -   Go SDK (`clients/go`) with examples and docs
    -   Python SDK (`clients/python`) distributed via `pip`
    -   Java SDK (`clients/java`) zero-dependency, Java 11+
-   Docker image (`volandoo/fluxiondb`) for single-command deployment

---

## Running the Server

### Docker (recommended)

```sh
# Pull the upstream image
docker pull volandoo/fluxiondb:latest

# Run the server
docker run --init --rm -it -p 8080:8080 \
    -v $(pwd)/tmp_data:/app/data volandoo/fluxiondb:latest \
    --secret-key=YOUR_SECRET_KEY --data=/app/data
```

To build the image from source:

```sh
make build
SECRET_KEY=YOUR_SECRET_KEY make run
```

### Native Build (Qt 6)

```sh
qmake6 fluxiondb.pro
make
./fluxiondb --secret-key=YOUR_SECRET_KEY --data=/path/to/data
```

Arguments:

-   `--secret-key` (required) secures client connections.
-   `--data` (optional) enables persistence by pointing to a writable directory. Omit to run fully in-memory.

---

## WebSocket Protocol Overview

All requests are JSON documents. Clients authenticate during the WebSocket handshake by providing an `api-key` query parameter in the connection URL (e.g., `ws://localhost:8080?api-key=YOUR_KEY`). Supply the master key for full access, or a scoped key that you created with the `keys` management message. The legacy `auth` message is no longer accepted by the server.

| Type      | Purpose                                                        |
| --------- | -------------------------------------------------------------- |
| `ins`     | Insert one or more records                                     |
| `qry`     | Fetch latest record per document                               |
| `cols`    | List collections                                               |
| `qdoc`    | Fetch records for a document                                   |
| `ddoc`    | Delete a document                                              |
| `dcol`    | Delete a collection                                            |
| `drec`    | Delete a single record                                         |
| `dmrec`   | Delete multiple records                                        |
| `drrng`   | Delete records within a timestamp range                        |
| `sval`    | Set key-value entry                                            |
| `gval`    | Get key-value entry                                            |
| `gvalues` | Get key-value entries by literal key or `/regex/`              |
| `rval`    | Remove key-value entry                                         |
| `gvals`   | Fetch all key-value entries                                    |
| `gkeys`   | Fetch all keys                                                 |
| `keys`    | Manage API keys (add/remove scoped keys)                       |
| `conn`    | List active client connections (IP, elapsed ms, optional name) |

The `doc` field in `qry` requests and the `key` field in `gvalues` requests both accept `/pattern/flags` strings (e.g. `/device-.*/i`). Literal values continue to work as before; the server only compiles the expression when the payload starts with `/` and contains a trailing `/`.

Clients may also include an optional `name` query parameter during the WebSocket handshake (`?api-key=...&name=my-sdk`). The server echoes that label in `conn` responses so you can tell which socket is which.

### API Key Scopes

The built-in master key always has full access, cannot be removed, and is the only credential allowed to create or revoke other keys. Scoped keys created via the `keys` message support three levels:

-   `readonly` – query-only access.
-   `read_write` – read and insert/update operations.
-   `read_write_delete` – full access, including destructive operations.

All clients handle the JSON marshalling for you, as well as appending the API key to the connection URL.

### Managing API Keys Manually

To create or revoke scoped keys without an SDK, send a `keys` message after the socket is established (using the master key in the handshake):

```json
{
    "id": "create-1",
    "type": "keys",
    "data": "{\"action\":\"add\",\"key\":\"analytics-reader\",\"scope\":\"readonly\"}"
}
```

Removing a key works the same way:

```json
{
    "id": "remove-1",
    "type": "keys",
    "data": "{\"action\":\"remove\",\"key\":\"analytics-reader\"}"
}
```

Responses include the original `id` plus a `"status": "ok"` field on success, or `"error"` when the operation is rejected. The master key always retains `read_write_delete` scope and cannot be removed.

---

## Node.js Client

Install from npm:

```sh
npm install @volandoo/fluxiondb-client
```

Example:

```ts
import { FluxionDBClient } from "@volandoo/fluxiondb-client";

const client = new FluxionDBClient({ url: "ws://localhost:8080", apiKey: "YOUR_SECRET_KEY" });

const now = Math.floor(Date.now() / 1000);
await client.insertMultipleRecords([
    { ts: now, doc: "device-1", data: JSON.stringify({ temperature: 22.5 }), col: "sensors" },
]);

const collections = await client.fetchCollections();
console.log(collections);

// Match a subset of documents using /regex/ patterns
const latest = await client.fetchLatestRecords({
    col: "sensors",
    ts: now,
    doc: "/device-[12]/",
});
console.log(latest);

const history = await client.fetchDocument({
    col: "sensors",
    doc: "device-1",
    from: now - 3600,
    to: now,
});
console.log(history);

// Fetch key/value entries using a literal key or /regex/
const envConfig = await client.getValues({
    col: "config",
    key: "/env\\..*/",
});
console.log(envConfig);

const connections = await client.getConnections();
console.log(connections);

client.close();
```

Scoped access can be provisioned at runtime:

```ts
await client.addApiKey({ key: "sensor-reader", scope: "readonly" });
await client.removeApiKey({ key: "sensor-reader" });
```

Call `client.getValues({ col })` to dump every key/value pair, or include `key` (literal or `/regex/`) to let the server filter before sending results.

The full TypeScript source, tests, and build tooling live under `clients/node/`.

---

## Go Client

Add the module:

```sh
go get github.com/volandoo/fluxiondb/clients/go
```

Example:

```go
client := fluxiondb.NewClient("ws://localhost:8080", "YOUR_SECRET_KEY")
if err := client.Connect(); err != nil {
    log.Fatal(err)
}
defer client.Close()

now := time.Now().Unix()
_ = client.InsertSingleDocumentRecord(fluxiondb.InsertMessageRequest{
    TS:   now,
    Doc:  "device-1",
    Data: `{"temperature":22.5}`,
    Col:  "sensors",
})

latest, _ := client.FetchLatestRecords(fluxiondb.FetchLatestRecordsParams{
    Col: "sensors",
    TS:  now,
    Doc: "/device-[12]/",
})
fmt.Println(latest)

pattern := "/env\\..*/"
envVars, _ := client.GetValues(fluxiondb.GetValuesParams{
    Col: "config",
    Key: &pattern,
})
fmt.Println(envVars)

connections, _ := client.GetConnections()
fmt.Println(connections)
```

See `clients/go/README.md` and the example app in `clients/go/example/` for more.

Omit the `Key` pointer when calling `client.GetValues` to return the entire collection, or set it to a literal/`/regex/` string to reduce the payload.

---

## Java Client

### Installation

**Maven (via JitPack):**
```xml
<repositories>
    <repository>
        <id>jitpack.io</id>
        <url>https://jitpack.io</url>
    </repository>
</repositories>

<dependencies>
    <dependency>
        <groupId>com.github.volandoo</groupId>
        <artifactId>fluxiondb</artifactId>
        <version>main-SNAPSHOT</version>
        <classifier>java-client</classifier>
    </dependency>
</dependencies>
```

**Gradle (via JitPack):**
```gradle
repositories {
    maven { url 'https://jitpack.io' }
}

dependencies {
    implementation 'com.github.volandoo:fluxiondb:main-SNAPSHOT:java-client'
}
```

**Or download JAR from [GitHub Releases](https://github.com/volandoo/fluxiondb/releases)**

### Example Usage

```java
import com.volandoo.fluxiondb.*;
import com.volandoo.fluxiondb.model.requests.*;
import com.volandoo.fluxiondb.model.responses.*;

FluxionDBClient client = new FluxionDBClientBuilder()
    .url("ws://localhost:8080")
    .apiKey("YOUR_SECRET_KEY")
    .connectionName("java-client")
    .build();

client.connect().get();

long now = System.currentTimeMillis() / 1000;
client.insertSingleRecord(new InsertMessageRequest(
    now,
    "device-1",
    "{\"temperature\":22.5}",
    "sensors"
)).get();

FetchLatestRecordsParams params = FetchLatestRecordsParams.builder()
    .col("sensors")
    .ts(now)
    .doc("/device-[12]/")
    .build();

Map<String, RecordResponse> latest = client.fetchLatestRecords(params).get();
System.out.println(latest);

GetValuesParams kvParams = new GetValuesParams("config", "/env\\..*/");
Map<String, String> envVars = client.getValues(kvParams).get();
System.out.println(envVars);

List<ConnectionInfo> connections = client.getConnections().get();
System.out.println(connections);

client.closeAsync().get();
```

The Java client uses CompletableFuture for async operations and supports try-with-resources:

```java
try (FluxionDBClient client = new FluxionDBClientBuilder()
        .url("ws://localhost:8080")
        .apiKey("YOUR_SECRET_KEY")
        .build()) {

    client.connect().get();
    // Use client...
} // Auto-closes
```

See `clients/java/README.md` for installation and `clients/java/DOCUMENTATION.md` for the complete API reference.

---

## Development

-   `make build` – build the Docker image
-   `SECRET_KEY=dev make run` – run the server in Docker with persistence mounted at `tmp_data/`
-   `qmake6 fluxiondb.pro && make` – native build (requires Qt 6 Core + WebSockets)
-   `cd clients/node && npm install && npm run build` – build Node client bundle
-   `cd clients/go && go test ./...` – run Go client tests
-   `cd clients/python && pip install -e . && pytest` – run Python client tests
-   `cd clients/java && ./build.sh` – build Java client JAR (requires Java 11+)

### Repository Layout

-   `src/` – Qt/C++17 server sources
-   `clients/node/` – Node.js SDK (TypeScript)
-   `clients/go/` – Go SDK
-   `clients/python/` – Python SDK
-   `clients/java/` – Java SDK (Java 11+, zero dependencies)
-   `cli/` – cross-platform command-line client (see [CLI.md](CLI.md))

---

## License

Apache-2.0
