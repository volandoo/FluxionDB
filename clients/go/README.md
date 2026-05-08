# FluxionDB Go Client

Go client library for FluxionDB (Live Time Series Database).

## Installation

```bash
go get github.com/volandoo/fluxiondb/clients/go
```

## Usage

```go
package main

import (
    "fmt"
    "log"
    "time"

    fluxiondb "github.com/volandoo/fluxiondb/clients/go"
)

func main() {
    // Create a new client
    client := fluxiondb.NewClient("ws://localhost:8080", "YOUR_SECRET_KEY")
    client.SetConnectionName("docs-example-go")

    // Connect to the server
    if err := client.Connect(); err != nil {
        log.Fatal("Failed to connect:", err)
    }
    defer client.Close()

    if _, err := client.AddAPIKey("readonly-sdk", fluxiondb.ApiKeyScopeReadOnly); err != nil {
        log.Printf("Failed to add scoped key: %v", err)
    }

    // Insert a time series record
    now := time.Now().Unix()
    record := fluxiondb.InsertMessageRequest{
        TS:   now,
        Doc:  "device123",
        Data: `{"temperature": 22.5}`,
        Col:  "sensors",
    }

    if err := client.InsertSingleDocumentRecord(record); err != nil {
        log.Printf("Failed to insert record: %v", err)
        return
    }

    // Fetch all collections
    collections, err := client.FetchCollections()
    if err != nil {
        log.Printf("Failed to fetch collections: %v", err)
        return
    }
    fmt.Printf("Collections: %v\n", collections)

    connections, err := client.GetConnections()
    if err != nil {
        log.Printf("Failed to fetch connections: %v", err)
        return
    }
    fmt.Printf("Active connections: %v\n", connections)
    fmt.Println("Connections include name labels when SetConnectionName is used.")

    // Fetch records for a document in a collection
    records, err := client.FetchDocument(fluxiondb.FetchRecordsParams{
        Col:    "sensors",
        Doc:    "device123",
        From:   now - 3600, // 1 hour ago
        To:     now,
        Where:  "/state:(flying|landed)/", // optional: substring or /regex/flags include predicate
        Filter: "quality:bad",             // optional: substring or /regex/flags exclude predicate
    })
    if err != nil {
        log.Printf("Failed to fetch records: %v", err)
        return
    }

    fmt.Printf("Records: %v\n", records)

    // Fetch latest record per document using /regex/ filters
    from := now - 3600
    sessions, err := client.FetchLatestRecords(fluxiondb.FetchLatestRecordsParams{
        Col:    "sensors",
        TS:     now,
        Doc:    "/device[0-9]+/",
        From:   &from,
        Where:  "state:flying",
        Filter: "quality:bad",
    })
    if err != nil {
        log.Printf("Failed to fetch latest records: %v", err)
        return
    }
    fmt.Printf("Latest records: %v\n", sessions)

    // Fetch regex-based key/value pairs (omit Key to fetch entire collection)
    pattern := "/env\\..*/"
    envConfigs, err := client.GetValues(fluxiondb.GetValuesParams{
        Col: "config",
        Key: &pattern,
    })
    if err != nil {
        log.Printf("Failed to fetch env configs: %v", err)
        return
    }
fmt.Printf("Env configs: %v\n", envConfigs)
}
```

> Prefer a single call? Use `fluxiondb.NewClientWithName(url, apiKey, "my-name")` to set the connection label up front.

## API Reference

### Time Series Methods

-   `InsertSingleDocumentRecord(record InsertMessageRequest) error` - Insert a single record into a document
-   `InsertMultipleRecords(records []InsertMessageRequest) error` - Insert multiple records into a document
-   `FetchDocument(params FetchRecordsParams) ([]RecordResponse, error)` - Fetch records for a specific document, optionally keeping records with `Where` and excluding records with `Filter`
-   `FetchLatestRecords(params FetchLatestRecordsParams) (map[string]RecordResponse, error)` - Fetch the latest record per document in a collection, optionally keeping records with `Where` and excluding records with `Filter`
-   `DeleteDocument(params DeleteDocumentParams) error` - Delete a document (optionally across collections)
-   `DeleteDocumentRecord(params DeleteRecord) error` - Delete a single record within a document
-   `DeleteMultipleRecords(params []DeleteRecord) error` - Delete multiple records within documents
-   `DeleteRecordsRange(params DeleteRecordsRange) error` - Delete all records within a timestamp range for a document

### Collection Methods

-   `FetchCollections() ([]string, error)` - Get all collections
-   `DeleteCollection(params DeleteCollectionParams) error` - Delete a collection

### Connection Insights

-   `SetConnectionName(name string)` - Tag the connection; echoed in `GetConnections`
-   `GetConnections() ([]ConnectionInfo, error)` - List active WebSocket clients (IP, elapsed ms, label, and whether it's the caller)

### Key-Value Methods

-   `SetValue(params SetValueParams) error` - Set a key-value pair
-   `GetValue(params GetValueParams) (string, error)` - Get a value by key
-   `GetValues(params GetValuesParams) (map[string]string, error)` - Fetch key-value pairs for an entire collection or filter by literal/`/regex/` key
-   `GetKeys(params CollectionParam) ([]string, error)` - Get all keys in a collection
-   `DeleteValue(params DeleteValueParams) error` - Delete a key-value pair

### API Key Management

-   `AddAPIKey(key string, scope ApiKeyScope) (ManageAPIKeyResponse, error)` - Create a scoped key
-   `RemoveAPIKey(key string) (ManageAPIKeyResponse, error)` - Revoke a scoped key

> **Note:** Only the master API key (supplied via the `api-key` query parameter during the handshake) is allowed to add or remove keys. Scoped keys can consume their allotted permissions but cannot manage other credentials.

## Important Notes

-   Timestamps are stored as 64-bit numbers; choose the unit that fits your workload (seconds, milliseconds, counters, etc.).
-   Pick a consistent unit per collection to keep queries intuitive.
-   The client automatically appends the `api-key` query parameter and manages connection retries
-   All data fields should be JSON-encoded strings when storing complex objects

## Error Handling

The client includes automatic reconnection with exponential backoff. Connection errors are handled gracefully, and the client will attempt to reconnect up to 5 times before giving up.

## License

Apache-2.0
