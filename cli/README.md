# FluxionDB CLI

Command-line tool for interacting with FluxionDB over WebSockets using the official Go client.

## Quick start

```sh
go build -o fluxiondb ./...
./fluxiondb fetch-collections --url wss://localhost:8080 --apikey YOUR_KEY
```

You can also pass an optional connection label:

```sh
./fluxiondb fetch-latest --url wss://localhost:8080 --apikey YOUR_KEY \
  --col sensors --ts $(date +%s) --doc "/device-.*/" --name "cli-agent"
```

## Commands (high level)

- `fetch-collections` – list collections.
- `fetch-latest` – latest record per document (with optional regex filters).
- `fetch-document` – records for a document within a time window.
- `insert-record` – insert one record (raw JSON payload).
- `set-value` / `get-values` / `delete-value` / `list-keys` – key/value operations.
- `delete-document` / `delete-collection` – destructive actions.
- `connections` – list active sockets.
- `add-key` / `remove-key` – manage scoped API keys (requires master key).

All commands share `--url`, `--apikey`, and optional `--name` flags. See `--help` on any subcommand for details.
