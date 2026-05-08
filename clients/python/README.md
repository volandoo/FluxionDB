# FluxionDB Python Client

This package provides an officially supported Python interface to FluxionDB that mirrors the Node.js and Go SDKs. It wraps the WebSocket protocol (query-parameter authentication, typed message payloads, reconnect handling) and exposes the same record, collection, key/value, and API key management helpers.

## Installation

```sh
cd clients/python
pip install .
```

For development:

```sh
pip install -e ".[dev]"
```

## Usage

```python
from fluxiondb_client import FluxionDBClient, ApiKeyScope

client = FluxionDBClient(
    url="ws://localhost:8080",
    api_key="YOUR_SECRET_KEY",
    connection_name="python-sdk",
)
client.connect()

now = int(time.time())
client.insert_multiple_records([
    {"ts": now, "doc": "device-1", "data": '{"temperature":22.5}', "col": "sensors"},
])
latest = client.fetch_latest_records({
    "col": "sensors",
    "ts": now,
    "doc": "/device-[12]/",
    "where": "state:flying",  # Optional: keep records containing this string
    "filter": "quality:bad",  # Optional: drop records containing this string
})
print(latest)

client.add_api_key("sensor-reader", ApiKeyScope.READ_ONLY)
client.remove_api_key("sensor-reader")
client.close()
```

Refer to `fluxiondb_client/client.py` for the full list of helpers, all of which map directly to the operations already available in the Go and Node clients.

## Tests

```sh
cd clients/python
pytest
```

Or run against a real FluxionDB instance via Docker Compose (builds the server and test container):

```sh
docker compose up --build python-client
```

The tests expect `FLUXIONDB_URL` and `FLUXIONDB_API_KEY`; when using Compose these default to `ws://fluxiondb:8080` and `dev-secret`. Set them manually if you are running the server yourself.

## Publishing

Version bumps in `pyproject.toml` trigger the `python-client-release` GitHub Actions workflow when merged to `main`. The workflow installs the package, builds the wheel/sdist via `python -m build`, and uploads the artifacts to PyPI using the `PYPI_API_TOKEN` secret before tagging the release.
