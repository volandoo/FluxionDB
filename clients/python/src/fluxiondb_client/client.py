from __future__ import annotations

import json
import logging
import threading
import time
import uuid
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, cast
from urllib.parse import parse_qsl, urlencode, urlparse, urlunparse

import websocket

from .exceptions import (
    FluxionDBConnectionError,
    FluxionDBError,
    FluxionDBTimeoutError,
)
from .types import (
    ApiKeyScope,
    CollectionParam,
    CollectionsResponse,
    ConnectionInfo,
    ConnectionsResponse,
    DeleteCollectionParams,
    DeleteDocumentParams,
    DeleteRecord,
    DeleteRecordsRange,
    DeleteValueParams,
    FetchLatestRecordsParams,
    FetchRecordsParams,
    GetValueParams,
    GetValuesParams,
    InsertMessageRequest,
    InsertMessageResponse,
    KeyValueAllKeysResponse,
    KeyValueAllValuesResponse,
    KeyValueResponse,
    ManageApiKeyParams,
    ManageApiKeyResponse,
    QueryCollectionResponse,
    QueryResponse,
    RecordResponse,
    SetValueParams,
)

logger = logging.getLogger(__name__)

MESSAGE_TYPES = {
    "INSERT": "ins",
    "QUERY_RECORDS": "qry",
    "QUERY_COLLECTIONS": "cols",
    "QUERY_DOCUMENT": "qdoc",
    "DELETE_DOCUMENT": "ddoc",
    "DELETE_COLLECTION": "dcol",
    "DELETE_RECORD": "drec",
    "DELETE_MULTIPLE_RECORDS": "dmrec",
    "DELETE_RECORDS_RANGE": "drrng",
    "SET_VALUE": "sval",
    "GET_VALUE": "gval",
    "GET_VALUES": "gvalues",
    "REMOVE_VALUE": "rval",
    "GET_ALL_VALUES": "gvals",
    "GET_ALL_KEYS": "gkeys",
    "MANAGE_API_KEYS": "keys",
    "CONNECTIONS": "conn",
}


@dataclass
class _PendingRequest:
    event: threading.Event = field(default_factory=threading.Event)
    response: Optional[Dict[str, Any]] = None
    error: Optional[Exception] = None


class FluxionDBClient:
    def __init__(
        self,
        url: str,
        api_key: str,
        *,
        connection_name: Optional[str] = None,
        reconnect_interval: float = 5.0,
        max_reconnect_attempts: int = 5,
        request_timeout: float = 30.0,
        show_logs: bool = False,
    ) -> None:
        self._url = url
        self._api_key = api_key
        self._connection_name = connection_name
        self._reconnect_interval = reconnect_interval
        self._max_reconnect_attempts = max_reconnect_attempts
        self._request_timeout = request_timeout
        self._show_logs = show_logs

        self._ws_app: Optional[websocket.WebSocketApp] = None
        self._ws_thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()
        self._ready_event = threading.Event()
        self._should_reconnect = True
        self._reconnect_attempts = 0
        self._lock = threading.RLock()
        self._pending: Dict[str, _PendingRequest] = {}

    def set_connection_name(self, name: str) -> None:
        self._connection_name = name

    def connect(self, timeout: float = 10.0) -> None:
        with self._lock:
            if self._ready_event.is_set() and self._ws_app:
                return

            if self._ws_thread and self._ws_thread.is_alive():
                # A connection attempt is already running.
                pass
            else:
                self._stop_event.clear()
                self._should_reconnect = True
                self._reconnect_attempts = 0
                self._ready_event.clear()
                self._ws_app = self._create_websocket_app()
                self._ws_thread = threading.Thread(
                    target=self._run_websocket_loop, daemon=True
                )
                self._ws_thread.start()

        if not self._ready_event.wait(timeout):
            raise FluxionDBConnectionError(
                "Timed out waiting for FluxionDB ready message"
            )

    def close(self) -> None:
        with self._lock:
            self._should_reconnect = False
            self._stop_event.set()
            if self._ws_app:
                try:
                    self._ws_app.close()
                finally:
                    self._ws_app = None
            thread = self._ws_thread
        if thread and thread.is_alive():
            thread.join(timeout=2.0)
        self._ready_event.clear()
        self._fail_pending(FluxionDBError("Connection closed"))

    def fetch_collections(self) -> List[str]:
        response = self._send_request(MESSAGE_TYPES["QUERY_COLLECTIONS"], {})
        typed = cast(CollectionsResponse, response)
        return typed.get("collections", [])

    def get_connections(self) -> List[ConnectionInfo]:
        response = self._send_request(MESSAGE_TYPES["CONNECTIONS"], {})
        typed = cast(ConnectionsResponse, response)
        return typed.get("connections", [])

    def delete_collection(self, params: DeleteCollectionParams) -> None:
        self._send_request(MESSAGE_TYPES["DELETE_COLLECTION"], params)

    def fetch_latest_records(
        self, params: FetchLatestRecordsParams
    ) -> Dict[str, RecordResponse]:
        payload: Dict[str, Any] = {"col": params["col"], "ts": params["ts"]}
        if "doc" in params and params["doc"]:
            payload["doc"] = params["doc"]
        if "from" in params:
            payload["from"] = params["from"]
        if "where" in params:
            payload["where"] = params["where"]
        if "filter" in params:
            payload["filter"] = params["filter"]
        response = self._send_request(MESSAGE_TYPES["QUERY_RECORDS"], payload)
        typed = cast(QueryResponse, response)
        return typed.get("records", {})

    def insert_multiple_records(
        self, items: List[InsertMessageRequest]
    ) -> InsertMessageResponse:
        return cast(
            InsertMessageResponse,
            self._send_request(MESSAGE_TYPES["INSERT"], items),
        )

    def insert_single_record(
        self, item: InsertMessageRequest
    ) -> InsertMessageResponse:
        return self.insert_multiple_records([item])

    def fetch_document(
        self, params: FetchRecordsParams
    ) -> List[RecordResponse]:
        payload: Dict[str, Any] = {
            "col": params["col"],
            "doc": params["doc"],
            "from": params["from"],
            "to": params["to"],
        }
        if "limit" in params:
            payload["limit"] = params["limit"]
        if "reverse" in params:
            payload["reverse"] = params["reverse"]
        if "where" in params:
            payload["where"] = params["where"]
        if "filter" in params:
            payload["filter"] = params["filter"]
        response = self._send_request(MESSAGE_TYPES["QUERY_DOCUMENT"], payload)
        typed = cast(QueryCollectionResponse, response)
        return typed.get("records", [])

    def delete_document(self, params: DeleteDocumentParams) -> None:
        self._send_request(MESSAGE_TYPES["DELETE_DOCUMENT"], params)

    def delete_record(self, params: DeleteRecord) -> None:
        self._send_request(MESSAGE_TYPES["DELETE_RECORD"], params)

    def delete_multiple_records(self, params: List[DeleteRecord]) -> None:
        self._send_request(MESSAGE_TYPES["DELETE_MULTIPLE_RECORDS"], params)

    def delete_records_range(self, params: DeleteRecordsRange) -> None:
        self._send_request(MESSAGE_TYPES["DELETE_RECORDS_RANGE"], params)

    def set_value(self, params: SetValueParams) -> None:
        self._send_request(MESSAGE_TYPES["SET_VALUE"], params)

    def get_value(self, params: GetValueParams) -> str:
        response = self._send_request(MESSAGE_TYPES["GET_VALUE"], params)
        typed = cast(KeyValueResponse, response)
        return typed["value"]

    def get_keys(self, params: CollectionParam) -> List[str]:
        response = self._send_request(MESSAGE_TYPES["GET_ALL_KEYS"], params)
        typed = cast(KeyValueAllKeysResponse, response)
        return typed.get("keys", [])

    def get_values(self, params: GetValuesParams) -> Dict[str, str]:
        msg_type = (
            MESSAGE_TYPES["GET_VALUES"]
            if "key" in params and params["key"]
            else MESSAGE_TYPES["GET_ALL_VALUES"]
        )
        response = self._send_request(msg_type, params)
        typed = cast(KeyValueAllValuesResponse, response)
        return typed.get("values", {})

    def delete_value(self, params: DeleteValueParams) -> None:
        self._send_request(MESSAGE_TYPES["REMOVE_VALUE"], params)

    def add_api_key(self, key: str, scope: ApiKeyScope) -> ManageApiKeyResponse:
        payload: ManageApiKeyParams = {
            "action": "add",
            "key": key,
            "scope": scope,
        }
        return cast(
            ManageApiKeyResponse,
            self._send_request(MESSAGE_TYPES["MANAGE_API_KEYS"], payload),
        )

    def remove_api_key(self, key: str) -> ManageApiKeyResponse:
        payload: ManageApiKeyParams = {
            "action": "remove",
            "key": key,
        }
        return cast(
            ManageApiKeyResponse,
            self._send_request(MESSAGE_TYPES["MANAGE_API_KEYS"], payload),
        )

    def _create_websocket_app(self) -> websocket.WebSocketApp:
        url = self._build_authenticated_url()
        if self._show_logs:
            logger.info("Connecting to %s", url)
        return websocket.WebSocketApp(
            url,
            on_open=self._on_open,
            on_message=self._on_message,
            on_close=self._on_close,
            on_error=self._on_error,
        )

    def _run_websocket_loop(self) -> None:
        while not self._stop_event.is_set():
            app = self._ws_app
            if not app:
                return
            try:
                app.run_forever()
            except Exception as exc:
                logger.warning("WebSocket run loop error: %s", exc)
            if self._stop_event.is_set() or not self._should_reconnect:
                break
            self._reconnect_attempts += 1
            if self._reconnect_attempts > self._max_reconnect_attempts:
                logger.error("Max reconnection attempts exceeded")
                self._fail_pending(
                    FluxionDBConnectionError(
                        "Exceeded maximum reconnection attempts"
                    )
                )
                break
            if self._show_logs:
                logger.warning(
                    "Reconnecting to FluxionDB... attempt %s/%s",
                    self._reconnect_attempts,
                    self._max_reconnect_attempts,
                )
            time.sleep(self._reconnect_interval)
            with self._lock:
                if self._stop_event.is_set() or not self._should_reconnect:
                    break
                self._ready_event.clear()
                self._ws_app = self._create_websocket_app()

    def _on_open(self, _ws: websocket.WebSocketApp) -> None:
        if self._show_logs:
            logger.info("WebSocket connected, awaiting authentication...")

    def _on_message(self, _ws: websocket.WebSocketApp, message: str) -> None:
        try:
            payload = json.loads(message)
        except json.JSONDecodeError:
            logger.warning("Received non-JSON payload: %s", message)
            return

        msg_type = payload.get("type")
        if msg_type == "ready":
            if self._show_logs:
                logger.info("Authentication successful")
            self._ready_event.set()
            self._reconnect_attempts = 0
            return

        message_id = payload.get("id")
        if not message_id:
            logger.warning("Received message without ID: %s", payload)
            return

        with self._lock:
            pending = self._pending.pop(message_id, None)
        if pending:
            pending.response = payload
            pending.event.set()
        else:
            logger.warning("Received unexpected response id=%s", message_id)

    def _on_close(
        self, _ws: websocket.WebSocketApp, status_code: int, msg: str
    ) -> None:
        if self._show_logs:
            logger.info("WebSocket closed (%s): %s", status_code, msg)
        self._ready_event.clear()
        self._fail_pending(FluxionDBError("Connection closed"))

    def _on_error(self, _ws: websocket.WebSocketApp, error: Any) -> None:
        logger.error("WebSocket error: %s", error)

    def _send_request(self, message_type: str, data: Any) -> Dict[str, Any]:
        self.connect()

        app = self._ws_app
        if not app:
            raise FluxionDBConnectionError("WebSocket not initialized")

        message_id = uuid.uuid4().hex
        pending = _PendingRequest()
        with self._lock:
            self._pending[message_id] = pending
        payload = {
            "id": message_id,
            "type": message_type,
            "data": json.dumps(data) if not isinstance(data, str) else data,
        }
        try:
            app.send(json.dumps(payload))
        except Exception as exc:
            with self._lock:
                self._pending.pop(message_id, None)
            pending.error = exc
            pending.event.set()
            raise FluxionDBError(f"Failed to send message: {exc}") from exc

        if not pending.event.wait(self._request_timeout):
            with self._lock:
                self._pending.pop(message_id, None)
            raise FluxionDBTimeoutError("Request timed out waiting for response")

        if pending.error:
            raise pending.error

        assert pending.response is not None
        if "error" in pending.response:
            raise FluxionDBError(str(pending.response["error"]))
        return pending.response

    def _fail_pending(self, error: Exception) -> None:
        with self._lock:
            pending_items = list(self._pending.values())
            self._pending.clear()
        for pending in pending_items:
            pending.error = error
            pending.event.set()

    def _build_authenticated_url(self) -> str:
        parsed = urlparse(self._url)
        query = dict(parse_qsl(parsed.query, keep_blank_values=True))
        query["api-key"] = self._api_key
        if self._connection_name:
            query["name"] = self._connection_name
        encoded = urlencode(query)
        return urlunparse(
            (
                parsed.scheme,
                parsed.netloc,
                parsed.path,
                parsed.params,
                encoded,
                parsed.fragment,
            )
        )
