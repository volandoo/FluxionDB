from __future__ import annotations

from typing import Dict, List, Literal, TypedDict

from typing_extensions import NotRequired

ApiKeyScope = Literal["readonly", "read_write", "read_write_delete"]

InsertMessageRequest = TypedDict(
    "InsertMessageRequest",
    {
        "ts": int,
        "doc": str,
        "data": str,
        "col": str,
    },
)

ManageApiKeyParams = TypedDict(
    "ManageApiKeyParams",
    {
        "action": str,
        "key": str,
        "scope": NotRequired[str],
    },
)

ManageApiKeyResponse = TypedDict(
    "ManageApiKeyResponse",
    {
        "id": str,
        "status": NotRequired[str],
        "error": NotRequired[str],
        "scope": NotRequired[str],
    },
)

InsertMessageResponse = TypedDict(
    "InsertMessageResponse",
    {
        "id": str,
    },
)

RecordResponse = TypedDict(
    "RecordResponse",
    {
        "ts": int,
        "data": str,
    },
)

QueryResponse = TypedDict(
    "QueryResponse",
    {
        "id": str,
        "records": Dict[str, RecordResponse],
    },
)

QueryCollectionResponse = TypedDict(
    "QueryCollectionResponse",
    {
        "id": str,
        "records": List[RecordResponse],
    },
)

CollectionsResponse = TypedDict(
    "CollectionsResponse",
    {
        "id": str,
        "collections": List[str],
    },
)

KeyValueResponse = TypedDict(
    "KeyValueResponse",
    {
        "id": str,
        "value": str,
    },
)

KeyValueAllKeysResponse = TypedDict(
    "KeyValueAllKeysResponse",
    {
        "id": str,
        "keys": List[str],
    },
)

KeyValueAllValuesResponse = TypedDict(
    "KeyValueAllValuesResponse",
    {
        "id": str,
        "values": Dict[str, str],
    },
)

ConnectionInfo = TypedDict(
    "ConnectionInfo",
    {
        "ip": str,
        "since": int,
        "self": bool,
        "name": NotRequired[str],
    },
)

ConnectionsResponse = TypedDict(
    "ConnectionsResponse",
    {
        "id": str,
        "connections": List[ConnectionInfo],
    },
)

CollectionParam = TypedDict(
    "CollectionParam",
    {
        "col": str,
    },
)

FetchLatestRecordsParamsRequired = TypedDict(
    "FetchLatestRecordsParamsRequired",
    {
        "col": str,
        "ts": int,
    },
)

FetchLatestRecordsParamsOptional = TypedDict(
    "FetchLatestRecordsParamsOptional",
    {
        "doc": str,
        "from": int,
        "where": str,  # Optional substring or /regex/flags include predicate
        "filter": str,  # Optional substring or /regex/flags exclude predicate
    },
    total=False,
)


class FetchLatestRecordsParams(
    FetchLatestRecordsParamsRequired, FetchLatestRecordsParamsOptional
):
    pass


FetchRecordsParamsRequired = TypedDict(
    "FetchRecordsParamsRequired",
    {
        "col": str,
        "doc": str,
        "from": int,
        "to": int,
    },
)


class FetchRecordsParams(FetchRecordsParamsRequired, total=False):
    limit: int
    reverse: bool
    where: str  # Optional substring or /regex/flags include predicate
    filter: str  # Optional substring or /regex/flags exclude predicate


DeleteCollectionParams = TypedDict(
    "DeleteCollectionParams",
    {
        "col": str,
    },
)

DeleteDocumentParams = TypedDict(
    "DeleteDocumentParams",
    {
        "doc": str,
        "col": str,
    },
)

DeleteRecord = TypedDict(
    "DeleteRecord",
    {
        "doc": str,
        "col": str,
        "ts": int,
    },
)

DeleteRecordsRange = TypedDict(
    "DeleteRecordsRange",
    {
        "doc": str,
        "col": str,
        "fromTs": int,
        "toTs": int,
    },
)

SetValueParams = TypedDict(
    "SetValueParams",
    {
        "col": str,
        "key": str,
        "value": str,
    },
)

GetValueParams = TypedDict(
    "GetValueParams",
    {
        "col": str,
        "key": str,
    },
)


class GetValuesParams(CollectionParam, total=False):
    key: str


DeleteValueParams = TypedDict(
    "DeleteValueParams",
    {
        "col": str,
        "key": str,
    },
)

WebSocketMessage = TypedDict(
    "WebSocketMessage",
    {
        "id": str,
        "type": str,
        "data": str,
    },
)

WebSocketResponse = TypedDict(
    "WebSocketResponse",
    {
        "id": str,
        "status": NotRequired[str],
        "error": NotRequired[str],
    },
)

__all__ = [
    "ApiKeyScope",
    "CollectionParam",
    "CollectionsResponse",
    "ConnectionInfo",
    "ConnectionsResponse",
    "DeleteCollectionParams",
    "DeleteDocumentParams",
    "DeleteRecord",
    "DeleteRecordsRange",
    "DeleteValueParams",
    "FetchLatestRecordsParams",
    "FetchRecordsParams",
    "GetValueParams",
    "GetValuesParams",
    "InsertMessageRequest",
    "InsertMessageResponse",
    "KeyValueAllKeysResponse",
    "KeyValueAllValuesResponse",
    "KeyValueResponse",
    "ManageApiKeyParams",
    "ManageApiKeyResponse",
    "QueryCollectionResponse",
    "QueryResponse",
    "RecordResponse",
    "SetValueParams",
    "WebSocketMessage",
    "WebSocketResponse",
]
