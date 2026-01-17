# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FluxionDB is an in-memory time series database with optional disk persistence, exposed over WebSockets. The architecture consists of:

- **Server** (`src/`): Qt/C++17 WebSocket server with in-memory storage backed by SQLite persistence
- **Clients**: Official SDKs in Node.js (`clients/node/`), Go (`clients/go/`), and Python (`clients/python/`)
- **CLI** (`cli/`): Go-based command-line client for operations and data import/export
- **Dashboard** (`dashboard/`): SvelteKit-based web UI

## Development Commands

### Server (Qt/C++)

```sh
# Docker build and run (recommended)
make build
SECRET_KEY=my-secret make run

# Native build (requires Qt 6 Core + WebSockets)
qmake6 fluxiondb.pro
make
./fluxiondb --secret-key=YOUR_SECRET_KEY --data=/path/to/data
```

### Node.js Client

```sh
cd clients/node
npm install
npm run build        # Compile TypeScript to dist/
npm test            # Run tests (requires server at ws://localhost:8080)
```

### Go Client

```sh
cd clients/go
go build ./...
go test ./...       # Fast unit tests, no external dependencies
```

### Python Client

```sh
cd clients/python
pip install -e .
pytest
```

### CLI Tool

```sh
cd cli
go build -o fluxiondb ./...
./fluxiondb --help

# Or run directly
go run . fetch-collections --url ws://localhost:8080 --apikey YOUR_KEY
```

### Dashboard (SvelteKit)

```sh
cd dashboard
npm install
npm run dev         # Development server
npm run build       # Production build
```

## Server Architecture

### Core Components

- **WebSocket** (`websocket.{h,cpp}`): Connection handler, message router, authentication via query-parameter API keys, and permission enforcement. Manages client scopes (readonly, read_write, read_write_delete) and routes all message types (`ins`, `qry`, `cols`, `qdoc`, `ddoc`, etc.).

- **Collection** (`collection.{h,cpp}`): In-memory time series storage using `std::unordered_map<QString, std::vector<std::unique_ptr<DataRecord>>>` for document-keyed records. Supports regex queries, key-value operations, and bulk flushes to disk.

- **SqliteStorage** (`sqlitestorage.{h,cpp}`): Persistence layer managing SQLite databases per collection. Handles schema setup, transactions, and CRUD operations for both time series records and key-value pairs.

### Message Types

The WebSocket protocol uses short type identifiers in JSON messages:
- `ins` - Insert records
- `qry` - Fetch latest record per document (supports `/regex/` patterns)
- `cols` - List collections
- `qdoc` - Query document history
- `ddoc`, `dcol`, `drec`, `dmrec`, `drrng` - Delete operations
- `sval`, `gval`, `gvalues`, `rval`, `gvals`, `gkeys` - Key-value operations
- `keys` - Manage scoped API keys
- `conn` - List active connections

### Authentication & Authorization

- Clients authenticate via `?api-key=KEY` query parameter in WebSocket handshake
- Master key (from `--secret-key` flag) has full access and can manage other keys
- Scoped keys created via `keys` message with three levels: `readonly`, `read_write`, `read_write_delete`
- Permission checks in `hasPermission()` and `permissionForType()` enforce access control

### Data Flow

1. Client connects with API key in query string
2. `WebSocket::onNewConnection()` validates key and stores client scope
3. Messages arrive at `WebSocket::processMessage()` → `handleMessage()`
4. Type-specific handlers (`handleInsert`, `handleQuerySessions`, etc.) operate on `Collection` instances
5. `Collection` updates in-memory data structures
6. Timer (`m_flushTimer`) periodically calls `flushToDisk()` to persist via `SqliteStorage`

## Client SDK Architecture

### Node.js (`clients/node/`)

- TypeScript source in `src/`
- Compiles to `dist/` with type declarations
- Single `FluxionDBClient` class wrapping WebSocket connection
- Promise-based API with request/response correlation via message IDs
- Tests use Jest with server integration

### Go (`clients/go/`)

- `Client` struct with synchronous methods
- Uses gorilla/websocket for connection management
- Example app in `example/` demonstrates usage patterns
- Table-driven tests following Go idioms

### Python (`clients/python/`)

- Source in `src/fluxiondb_client/`
- Published to PyPI
- Uses websocket-client library
- Pytest-based test suite

## CLI Tool (`cli/`)

Built with Cobra for command structure. Key capabilities:
- **Export/Import**: Dump collections to JSON Lines format with key-value metadata
- **Queries**: Fetch collections, latest records, document history
- **Mutations**: Insert, delete, manage API keys
- **Layout**: Exports create `<dir>/<collection>/<doc>.jsonl` plus `key_value.json` and optional `apikeys.json`

Common pattern for development:
```sh
cd cli
go run . <command> --url ws://localhost:8080 --apikey KEY [flags]
```

## Code Style

### C++ (Server)

- 4-space indentation, braces on same line
- Qt headers before STL
- Member fields prefixed with `m_` (e.g., `m_clients`, `m_databases`)
- PascalCase classes, camelCase methods
- Use `std::unique_ptr` for owned resources, `QList` for Qt collections

### TypeScript (Node Client)

- ES modules with double quotes
- PascalCase classes, camelCase async methods
- Shared types in `types.ts`
- Promise-based async patterns

### Go (Go Client & CLI)

- Follow `gofmt` formatting
- Idiomatic naming conventions
- Document exported symbols
- Table-driven tests for multiple cases

## Testing

- **Server**: Integration tests via client SDKs. Run server with `--secret-key=test` before client tests.
- **Node**: `cd clients/node && npm test` (requires live server)
- **Go**: `cd clients/go && go test ./...` (self-contained unit tests)
- **Python**: `cd clients/python && pytest`
- **CLI**: `cd cli && go test ./...`

## Persistence & Data Layout

When `--data` flag is provided:
- SQLite databases created per collection in data directory
- Schema managed by `SqliteStorage::ensureSchema()`
- Flush interval configurable (default 15s)
- Key-value pairs stored alongside time series data
- API keys persisted separately

## Commit Style

Follow existing patterns: short, imperative, lowercase messages (e.g., `add websocket retry`, `fix timestamp range query`). Scope commits narrowly and mention paths when changes span server/clients.

## Security Notes

- Never commit actual secret keys; use placeholders
- `tmp_data/` contains local fixtures and may persist across Docker runs
- Review `tmp_data/config/key_value.json` before sharing to avoid leaking credentials
