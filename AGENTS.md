# Repository Guidelines

## Project Structure & Module Organization

-   `src/`: Qt Core/WebSocket C++17 server. Key modules: `websocket.*` (connection handling), `collection.*` (in-memory storage and persistence), and `keyvalue.*` (key-value API).
-   `fluxiondb.pro` defines the Qt build configuration; `Makefile` exposes Docker workflows. Shared fixtures live under `tmp_data/`.
-   `clients/node/`: TypeScript SDK compiled to `dist/`; integration helpers and tests live in `src/*.test.ts`.
-   `clients/go/`: Go module with unit tests and the `example/` demo app.

## Build, Test, and Development Commands

-   `make build` – build the Docker image `volandoo/fluxiondb`.
-   `SECRET_KEY=my-secret make run` – run the container on `:8080`, mounting `tmp_data/` for persistence.
-   Native build: `qmake fluxiondb.pro && make` (requires Qt 6 Core + WebSockets).
-   Node client: `cd clients/node && npm install && npm run build`.
-   Go client: `cd clients/go && go build ./...`.

## Coding Style & Naming Conventions

-   C++: 4-space indentation, braces on the same line, Qt headers before STL, and member fields prefixed with `m_`. Prefer PascalCase classes and camelCase methods.
-   TypeScript: ES module imports, double quotes, PascalCase classes, camelCase async methods, and shared DTOs centralised in `types.ts`.
-   Go: rely on `gofmt`, idiomatic naming, and document exported symbols that shape the public SDK.

## Testing Guidelines

-   Server logic is currently validated through client integrations; add focused Qt unit tests when extending core modules.
-   Node: `npm install` then `npx jest --config jest.config.js --runInBand`. Requires an server at `ws://localhost:8080` with the matching `--secret-key`.
-   Go: `go test ./...` runs fast unit tests with no external dependencies. Extend tests using the existing table-driven style.

## Commit & Pull Request Guidelines

-   Follow the existing log style: short, imperative, lower-case messages (e.g., `add websocket retry`).
-   Scope commits narrowly and mention affected server/client paths when changes span components.
-   PRs should describe the change, list validation commands, and link relevant issues. Call out protocol updates when adjusting WebSocket payloads.
-   Attach logs or screenshots only when behaviour is collection-visible.

## Security & Configuration Tips

-   Never commit real secret keys; use placeholders and provide them via the `--secret-key` flag or secrets management.
-   Data mounted in `tmp_data/` may persist across runs; scrub fixtures before publishing images or demos.
-   Review `tmp_data/config/key_value.json` before sharing artifacts to avoid leaking sample credentials.
