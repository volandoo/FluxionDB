---
name: fluxiondb-cli
description: Operate the FluxionDB command-line client (`cli/`) to query, mutate, export, or import data via the Go SDK over WebSockets. Use when asked to run `fluxiondb` commands, move collections between environments, or manage key/value pairs programmatically.
---

# FluxionDB CLI Skill

Use this skill whenever a task explicitly references the FluxionDB binary/CLI, needs structured data exports/imports, or calls for scripted interactions with a FluxionDB server. The binary lives under `cli/` and wraps the Go SDK.

## Environment Prerequisites

- Go ≥ 1.22 is required (already used by the repo).
- A reachable FluxionDB server URL (e.g., `ws://localhost:8080`) and API key.
- Optional: Set `GOCACHE=$(pwd)/.gocache` when sandbox permissions block the default Go build cache.

## Building / Running

For ad-hoc commands, run directly:

```bash
cd cli
go run . <command> --url ws://localhost:8080 --apikey my-secret-key [flags]
```

For repeated use:

```bash
cd cli
go build -o fluxiondb ./...
./fluxiondb --help
```

All subcommands share the persistent flags `--url`, `--apikey`, and optional `--name` (labels the WebSocket connection).

## Common Workflows

### Inspect Data

- List collections: `go run . fetch-collections ...`
- Latest per document: `go run . fetch-latest --col <col> --ts $(date +%s) [--doc PATTERN] [--from TS]`
- Document history: `go run . fetch-document --col <col> --doc <doc> --from TS --to TS [--limit N] [--reverse]`
- Key/value dump: `go run . get-values --col <col> [--key literal-or-/regex/]`

### Mutations

- Insert records: `insert-record --col <col> --doc <doc> --data '{"json":"payload"}' [--ts TS]`
- Delete data: `delete-document`, `delete-collection`
- Key/value ops: `set-value`, `delete-value`, `list-keys`
- API keys & telemetry: `api-keys`, `connections`

### Export / Import Collections

Export writes JSON Lines plus key/value metadata:

```bash
go run . export sessions \
  --out-dir tmp \
  --url ws://localhost:8080 \
  --apikey my-secret-key
```

Output layout: `tmp/sessions/<doc>.jsonl` (one `{ts, doc, data}` per line) and `tmp/sessions/key_value.json`.

Import replays the same structure (delete the destination collection first if you need a clean slate):

```bash
go run . import sessions \
  --in-dir tmp \
  --url ws://localhost:8080 \
  --apikey my-secret-key
```

Both commands emit JSON summaries suitable for logging or downstream automation.

## References & Tips

- The CLI help text (`./fluxiondb --help` or `go run . --help`) lists every subcommand.
- `cli/README.md` documents persistent flags and high-level usage—skim it when unsure about flag semantics.
- Tests: `cd cli && GOCACHE=$(pwd)/.gocache go test ./...`
- When scripting, prefer `go run . ...` to avoid stale binaries unless determinism is required.

