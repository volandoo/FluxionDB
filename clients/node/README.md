# FluxionDB Node.js Client

Official TypeScript/JavaScript client for FluxionDB (In Memory Time Series Database).

## Installation

```bash
npm install @volandoo/fluxiondb-client
```

### Usage

## Usage

```typescript
import { FluxionDBClient } from "@volandoo/fluxiondb-client";

const client = new FluxionDBClient({
    url: "ws://localhost:8080",
    apiKey: "YOUR_SECRET_KEY",
    connectionName: "docs-example",
    // Optional: control initial retry behaviour when the socket can't be reached
    maxReconnectAttempts: 5,
    reconnectInterval: 5000, // ms
});

await client.connect();

// Insert time series records
const now = Date.now();
await client.insertMultipleRecords([
    {
        ts: now,
        doc: "device-1",
        data: JSON.stringify({ temperature: 22.5 }),
        col: "sensors",
    },
    {
        ts: now,
        doc: "device-2",
        data: JSON.stringify({ temperature: 23.1 }),
        col: "sensors",
    },
]);

// Fetch all collections
const collections = await client.fetchCollections();
console.log("Collections:", collections);

// Fetch latest records for all documents in a collection (supports /regex/)
const latest = await client.fetchLatestRecords({
    col: "sensors",
    ts: Date.now(),
    doc: "/device-[12]/", // literal IDs also supported
});
console.log("Latest records:", latest);

// Fetch history for a specific document
const history = await client.fetchRecords({
    col: "sensors",
    doc: "device-1",
    from: now - 3600000, // 1 hour ago
    to: now,
});
console.log("History:", history);

// Fetch key/value entries using literal keys or /regex/
const env = await client.getValues({ col: "config", key: "/env\\..*/" });
console.log(env);

// Inspect active WebSocket connections (ip + ms since connected)
const connections = await client.getConnections();
console.log(connections);

// Delete records within a timestamp range
await client.deleteRecordsRange({
    col: "sensors",
    doc: "device-1",
    fromTs: now - 1800000,
    toTs: now - 900000,
});

// Clean up
await client.disconnect();
```

> Tip: supply `connectionName` when creating the client to label this socket in `getConnections()` responses.

## API Reference

### Connection Management

- `connect(): Promise<void>` - Connect to the FluxionDB server
- `disconnect(): Promise<void>` - Disconnect from the server
- `close(): void` - Close the connection (alias for disconnect)
- `getConnections(): Promise<ConnectionInfo[]>` - List active connections, including the caller

### Time Series Methods

- `insertMultipleRecords(items: InsertMessageRequest[]): Promise<InsertMessageResponse>` - Insert one or more records
- `fetchLatestRecords(params: FetchLatestRecordsParams): Promise<Record<string, RecordResponse>>` - Fetch the latest record per document in a collection
- `fetchRecords(params: FetchRecordsParams): Promise<RecordResponse[]>` - Fetch records for a specific document within a time range
- `deleteDocument(params: DeleteDocumentParams): Promise<void>` - Delete a document
- `deleteRecord(params: DeleteRecord): Promise<void>` - Delete a single record
- `deleteMultipleRecords(params: DeleteRecord[]): Promise<void>` - Delete multiple records
- `deleteRecordsRange(params: DeleteRecordsRange): Promise<void>` - Delete all records within a timestamp range

### Collection Methods

- `fetchCollections(): Promise<string[]>` - Get all collections
- `deleteCollection(params: DeleteCollectionParams): Promise<void>` - Delete a collection

### Key-Value Methods

- `setValue(params: SetValueParams): Promise<void>` - Set a key-value pair
- `getValue(params: GetValueParams): Promise<string>` - Get a value by key
- `getValues(params: GetValuesParams): Promise<Record<string, string>>` - Fetch all key/value pairs or filter by literal/`/regex/` key
- `getKeys(params: CollectionParam): Promise<string[]>` - Get all keys in a collection
- `removeValue(params: DeleteValueParams): Promise<void>` - Remove a key-value pair

### API Key Management

- `addApiKey(params: AddApiKeyParams): Promise<ManageApiKeyResponse>` - Create a scoped API key
- `removeApiKey(params: RemoveApiKeyParams): Promise<ManageApiKeyResponse>` - Revoke a scoped API key

> **Note:** Only the master API key (provided during client initialization) can manage other keys. Scoped keys can only use their granted permissions.

## Type Definitions

### InsertMessageRequest

```typescript
{
    ts: number; // Timestamp or sequence number
    doc: string; // Document identifier
    data: string; // JSON-encoded data
    col: string; // Collection name
}
```

### FetchLatestRecordsParams

```typescript
{
    col: string;     // Collection name
    ts: number;      // Query timestamp
    doc?: string;    // Optional: literal doc ID or `/regex/flags`
    from?: number;   // Optional: only include records after this ts
}
```

> Pass document filters as either literal IDs or `/regex/flags` strings (for example `/device-.*/i`) to let the server match multiple documents.

### FetchRecordsParams

```typescript
{
    col: string;     // Collection name
    doc: string;     // Document identifier
    from: number;    // Start timestamp
    to: number;      // End timestamp
    limit?: number;  // Optional: max records to return
    reverse?: boolean; // Optional: return in reverse order
}
```

### DeleteRecordsRange

```typescript
{
    doc: string; // Document identifier
    col: string; // Collection name
    fromTs: number; // Start of range (inclusive)
    toTs: number; // End of range (inclusive)
}
```

### GetValuesParams

```typescript
{
    col: string;    // Collection name
    key?: string;   // Optional: literal key or `/regex/flags`
}
```

Omit `key` to retrieve every key/value pair, or provide a literal/`/regex/` string to constrain the results on the server side.

### ConnectionInfo

```typescript
{
    ip: string;     // peer IP address
    since: number;  // milliseconds since the connection was established
    self: boolean;  // true when this entry corresponds to the current client
    name?: string | null; // optional label supplied during connect()
}
```

## API Key Scopes

When creating scoped keys with `addApiKey`, three permission levels are available:

- `"readonly"` - Query operations only
- `"read_write"` - Read and insert/update operations
- `"read_write_delete"` - Full access including delete operations

```typescript
await client.addApiKey({
    key: "analytics-reader",
    scope: "readonly",
});
```

## Connection & Error Handling

The client includes automatic reconnection with exponential backoff. It will retry up to 5 times with a 5-second interval between attempts. Failed requests will be rejected with an error.

## Important Notes

- The `ts` field is a generic numeric coordinate - use seconds, milliseconds, counters, or any monotonic value that fits your use case
- All `data` fields should be JSON-encoded strings when storing complex objects
- The client automatically manages the WebSocket connection and appends the API key to requests
- Choose consistent timestamp units per collection for intuitive queries

## Development

```bash
# Install dependencies
npm install

# Build the client
npm run build

# Run tests (requires FluxionDB server running on ws://localhost:8080)
npx jest --runInBand
```

## License

Apache-2.0
