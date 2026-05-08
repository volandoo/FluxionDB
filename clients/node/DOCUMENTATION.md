# Getting Started

Install the package and establish a WebSocket session using your server URL and API key:

```ts
import FluxionDBClient from "@volandoo/fluxiondb-client";

const client = new FluxionDBClient({
    url: "ws://localhost:8080",
    apiKey: process.env.FLUXIONDB_API_KEY ?? "your-api-key",
    connectionName: "docs-example", // optional label returned by getConnections()
});

await client.connect();
```

Every method automatically calls `connect()` if the socket is closed, but invoking it up front ensures the connection (and automatic reconnection logic) is ready before issuing database commands.

## client.connect()

Establishes the WebSocket connection. Call once during startup or before your first request.

```ts
await client.connect();
```

## client.close()

Stops automatic reconnection and closes the underlying socket. Use it during shutdown or test teardown.

```ts
client.close();
```

## client.fetchCollections()

Returns the list of collection names currently stored in FluxionDB.

```ts
const collections = await client.fetchCollections();
console.log(collections);
```

## client.getConnections()

Lists active WebSocket clients along with their IP address, optional `name`, milliseconds since they connected, and whether an entry corresponds to the caller.

```ts
const connections = await client.getConnections();
const me = connections.find((conn) => conn.self);
console.log(`Connected from ${me?.ip} for ${me?.since}ms as ${me?.name ?? "unnamed"}`);
```

## client.deleteCollection(params)

Deletes an entire collection and its documents.

```ts
await client.deleteCollection({ col: "temperature" });
```

## client.fetchLatestRecords(params)

Retrieves the most recent record per document within a collection. Optionally scope by document ID and lower timestamp bound. The `doc` field accepts literal identifiers or `/regex/flags` strings (e.g. `/device-.*/i`) to match multiple documents server-side.

Use `where` to keep only latest records whose `data` contains a plain string. Use `filter` to drop latest records whose `data` contains a plain string. When both are provided, records must match `where` and must not match `filter`.

```ts
const latest = await client.fetchLatestRecords({
    col: "temperature",
    ts: Date.now(),
    doc: "/device-.*/i", // literal value also supported
    from: Date.now() - 3600_000, // optional
    where: "state:flying", // optional include predicate
    filter: "quality:bad", // optional exclude predicate
});
console.log(latest["device-a"].data);
```

## client.insertMultipleRecords(items)

Bulk insert records (each record targets one document in a collection).

```ts
await client.insertMultipleRecords([
    {
        col: "temperature",
        doc: "device-a",
        ts: Date.now(),
        data: JSON.stringify({ value: 22.5 }),
    },
    {
        col: "temperature",
        doc: "device-b",
        ts: Date.now(),
        data: JSON.stringify({ value: 21.9 }),
    },
]);
```

## client.insertSingleRecord(item)

Convenience wrapper that inserts a single record.

```ts
await client.insertSingleRecord({
    col: "temperature",
    doc: "device-a",
    ts: Date.now(),
    data: JSON.stringify({ value: 22.5 }),
});
```

## client.fetchDocument(params)

Fetches records for a specific document using time window filters (and optional limit/reverse). `where` keeps records whose `data` contains a plain string; `filter` drops records whose `data` contains a plain string.

```ts
const history = await client.fetchDocument({
    col: "temperature",
    doc: "device-a",
    from: Date.now() - 3600_000,
    to: Date.now(),
    limit: 100,
    reverse: true,
    where: "state:flying",
    filter: "quality:bad",
});
```

## client.deleteDocument(params)

Removes an entire document (and its records) from a collection.

```ts
await client.deleteDocument({ col: "temperature", doc: "device-a" });
```

## client.deleteRecord(params)

Deletes a single record identified by timestamp.

```ts
await client.deleteRecord({
    col: "temperature",
    doc: "device-a",
    ts: 1717610400000,
});
```

## client.deleteMultipleRecords(params)

Deletes multiple records across one or more documents.

```ts
await client.deleteMultipleRecords([
    { col: "temperature", doc: "device-a", ts: 1717610400000 },
    { col: "temperature", doc: "device-b", ts: 1717610460000 },
]);
```

## client.deleteRecordsRange(params)

Deletes all records for a document within a timestamp range.

```ts
await client.deleteRecordsRange({
    col: "temperature",
    doc: "device-a",
    fromTs: Date.now() - 3600_000,
    toTs: Date.now(),
});
```

## client.setValue(params)

Stores a key/value pair inside the collection-scoped KV store.

```ts
await client.setValue({
    col: "metadata",
    key: "device-a:location",
    value: "warehouse-1",
});
```

## client.getValue(params)

Retrieves a single key/value entry.

```ts
const location = await client.getValue({
    col: "metadata",
    key: "device-a:location",
});
console.log(location);
```

## client.getKeys(params)

Lists all keys in a KV collection.

```ts
const keys = await client.getKeys({ col: "metadata" });
```

## client.getValues(params)

Returns a map of key/value pairs in a KV collection. Omit `key` to retrieve everything, or supply a literal/`/regex/` string to narrow the results before they are sent back.

```ts
// Fetch every entry
const values = await client.getValues({ col: "metadata" });

// Fetch only keys that match /env.*/
const envValues = await client.getValues({
    col: "metadata",
    key: "/env\\..*/",
});
console.log(envValues["env.prod"]);
```

## client.deleteValue(params)

Removes a key/value entry.

```ts
await client.deleteValue({
    col: "metadata",
    key: "device-a:location",
});
```

## client.addApiKey(params)

Creates a new API key with a specific scope.

```ts
const response = await client.addApiKey({
    key: "readonly-device",
    scope: "readonly",
});
console.log(response.status);
```

## client.removeApiKey(params)

Deletes an existing API key.

```ts
await client.removeApiKey({
    key: "readonly-device",
});
```
If you pass `connectionName` when instantiating the client, that label will appear in `client.getConnections()` responses so you can identify each socket.
