# FluxionDB CLI Skill

This skill teaches AI agents how to invoke the FluxionDB command‑line client that lives under `cli/`. Use it whenever you need to query, export, or import data against a running FluxionDB server.

## Requirements

- Go toolchain ≥ 1.22 (already vendored in this repo).
- FluxionDB server reachable via WebSockets plus an API key (master key required for destructive commands and exports/imports).

## Build / Run

Most interactions can be executed directly with `go run .` from `cli/`. Example:

```bash
cd cli
go run . fetch-collections \
  --url ws://localhost:8080 \
  --apikey my-secret-key
```

For repeated use, build once:

```bash
cd cli
go build -o fluxiondb ./...
./fluxiondb --help
```

All commands require `--url` (e.g. `ws://localhost:8080`) and `--apikey`. Optional `--name` tags the connection for observability.

## Common Tasks

### Inspect Data

- List collections: `go run . fetch-collections ...`
- Latest record per document: `go run . fetch-latest --col <col> --ts $(date +%s)`
- Full document history: `go run . fetch-document --col <col> --doc <doc> --from <ts> --to <ts>`

### Mutations

- Insert: `go run . insert-record --col <col> --doc <doc> --data '{"json":"payload"}' --ts $(date +%s)`
- Key/value ops: `set-value`, `get-values`, `list-keys`, `delete-value`.
- Deletions: `delete-document`, `delete-collection`.

### Export / Import

Export every document (JSONL) plus `key_value.json`:

```bash
go run . export sessions \
  --out-dir tmp \
  --url ws://localhost:8080 \
  --apikey my-secret-key
```

Import previously exported data (delete the target collection first if you need a clean slate):

```bash
go run . import sessions \
  --in-dir tmp \
  --url ws://localhost:8080 \
  --apikey my-secret-key
```

Each command emits a JSON summary for programmatic consumption.

## Testing

```bash
cd cli
GOCACHE=$(pwd)/.gocache go test ./...
```

Use a scratch `GOCACHE` path when sandbox restrictions block the default cache.

