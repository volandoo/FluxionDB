# Getting Started

Install the package, create a client with your WebSocket endpoint + API key, and connect:

```sh
pip install fluxiondb-client
```

```python
from fluxiondb_client import FluxionDBClient

client = FluxionDBClient(
    url="ws://localhost:8080",
    api_key="YOUR_SECRET_KEY",
    connection_name="docs-example",  # optional label shown in get_connections()
)
client.connect()
```

Every helper calls `connect()` automatically if the socket is closed, but establishing the session up front ensures the authentication handshake and reconnection logic are ready before running commands.

## client.connect()

Establishes the WebSocket connection.

```python
client.connect()
```

## client.close()

Stops automatic reconnection and closes the WebSocket. Call it during shutdown or test teardown.

```python
client.close()
```

## client.fetch_collections()

Returns the list of collection names currently stored in FluxionDB.

```python
collections = client.fetch_collections()
print(collections)
```

## client.get_connections()

Lists active WebSocket clients with their IP, optional `name`, milliseconds since connect, and whether an entry corresponds to the caller.

```python
connections = client.get_connections()
self_conn = next(c for c in connections if c["self"])
print(f"My connection: {self_conn}")
```

## client.delete_collection(params)

Deletes an entire collection and its documents.

```python
client.delete_collection({"col": "temperature"})
```

## client.fetch_latest_records(params)

Retrieves the most recent record per document within a collection. The `doc` field accepts literal IDs or `/regex/flags` strings.

Use `where` to keep only latest records whose `data` matches a predicate. Use `filter` to drop latest records whose `data` matches a predicate. Predicates are plain substring matches by default, or regex matches when written as `/pattern/flags`.

```python
latest = client.fetch_latest_records({
    "col": "temperature",
    "ts": int(time.time() * 1000),
    "doc": "/device-[12]/",
    "from": int(time.time() * 1000) - 3_600_000,
    "where": "/state:(flying|landed)/",
    "filter": "quality:bad",
})
print(latest["device-1"]["data"])
```

## client.insert_multiple_records(items)

Bulk insert records.

```python
client.insert_multiple_records([
    {"col": "temperature", "doc": "device-1", "ts": ts, "data": '{"value":22.5}'},
    {"col": "temperature", "doc": "device-2", "ts": ts, "data": '{"value":23.1}'},
])
```

## client.insert_single_record(item)

Convenience wrapper for a single record.

```python
client.insert_single_record({
    "col": "temperature",
    "doc": "device-1",
    "ts": ts,
    "data": '{"value":22.5}',
})
```

## client.fetch_document(params)

Fetches records for a specific document within a time range, with optional `limit`/`reverse`. `where` keeps records whose `data` matches a predicate; `filter` drops records whose `data` matches a predicate. Predicates are plain substring matches by default, or regex matches when written as `/pattern/flags`.

```python
history = client.fetch_document({
    "col": "temperature",
    "doc": "device-1",
    "from": ts - 3_600_000,
    "to": ts,
    "limit": 100,
    "reverse": True,
    "where": "/state:(flying|landed)/",
    "filter": "quality:bad",
})
```

## client.delete_document(params)

Removes a document and its records.

```python
client.delete_document({"col": "temperature", "doc": "device-1"})
```

## client.delete_record(params)

Deletes a single record identified by timestamp.

```python
client.delete_record({"col": "temperature", "doc": "device-1", "ts": ts})
```

## client.delete_multiple_records(params)

Deletes multiple records across documents.

```python
client.delete_multiple_records([
    {"col": "temperature", "doc": "device-1", "ts": ts1},
    {"col": "temperature", "doc": "device-2", "ts": ts2},
])
```

## client.delete_records_range(params)

Deletes all records for a document within a timestamp range.

```python
client.delete_records_range({
    "col": "temperature",
    "doc": "device-1",
    "fromTs": ts - 3_600,
    "toTs": ts,
})
```

## client.set_value(params)

Stores a key/value pair in the collection-scoped KV store.

```python
client.set_value({
    "col": "metadata",
    "key": "device-1:location",
    "value": "warehouse-1",
})
```

## client.get_value(params)

Retrieves a single key/value.

```python
value = client.get_value({"col": "metadata", "key": "device-1:location"})
```

## client.get_keys(params)

Lists keys in a KV collection.

```python
keys = client.get_keys({"col": "metadata"})
```

## client.get_values(params)

Returns key/value pairs for a collection. Omit `key` to fetch everything or supply a literal/`/regex/` filter.

```python
values = client.get_values({"col": "metadata"})
env = client.get_values({"col": "metadata", "key": "/env\\..*/"})
```

## client.delete_value(params)

Deletes a key/value entry.

```python
client.delete_value({"col": "metadata", "key": "device-1:location"})
```

## client.add_api_key(key, scope)

Creates a new API key with a specific scope (`readonly`, `read_write`, `read_write_delete`).

```python
resp = client.add_api_key("readonly-device", "readonly")
print(resp["status"])
```

## client.remove_api_key(key)

Revokes a previously created API key.

```python
client.remove_api_key("readonly-device")
```

> Only the master key (the one provided to the client) can manage other keys. Scoped keys are limited to their granted permissions.
