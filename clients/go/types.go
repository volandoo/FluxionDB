package fluxiondb

// InsertMessageRequest represents a request to insert a message
type InsertMessageRequest struct {
	TS   int64  `json:"ts"`
	Doc  string `json:"doc"`
	Data string `json:"data"`
	Col  string `json:"col"`
}

// ApiKeyScope enumerates supported API key scopes
type ApiKeyScope string

const (
	// ApiKeyScopeReadOnly allows read-only operations
	ApiKeyScopeReadOnly ApiKeyScope = "readonly"
	// ApiKeyScopeReadWrite allows read and write operations
	ApiKeyScopeReadWrite ApiKeyScope = "read_write"
	// ApiKeyScopeReadWriteDelete grants full access
	ApiKeyScopeReadWriteDelete ApiKeyScope = "read_write_delete"
)

// ManageAPIKeyParams represents a manage-API-key request payload
type ManageAPIKeyParams struct {
	Action string      `json:"action"`
	Key    string      `json:"key"`
	Scope  ApiKeyScope `json:"scope,omitempty"`
}

// APIKeyInfo describes a stored API key entry
type APIKeyInfo struct {
	Key       string `json:"key"`
	Scope     string `json:"scope"`
	Deletable bool   `json:"deletable"`
}

// ManageAPIKeyResponse represents the response for API key management
type ManageAPIKeyResponse struct {
	ID     string       `json:"id"`
	Status string       `json:"status,omitempty"`
	Error  string       `json:"error,omitempty"`
	Scope  string       `json:"scope,omitempty"`
	Keys   []APIKeyInfo `json:"keys,omitempty"`
}

// InsertMessageResponse represents the response to an insert message
type InsertMessageResponse struct {
	ID string `json:"id"`
}

// QueryResponse represents the response to a query
type QueryResponse struct {
	ID      string                    `json:"id"`
	Records map[string]RecordResponse `json:"records"`
}

// RecordResponse represents a single record in a query response
type RecordResponse struct {
	TS   int64  `json:"ts"`
	Data string `json:"data"`
}

// QueryCollectionResponse represents the response to a collection query
type QueryCollectionResponse struct {
	ID      string           `json:"id"`
	Records []RecordResponse `json:"records"`
}

// CollectionsResponse represents the response containing collections
type CollectionsResponse struct {
	ID          string   `json:"id"`
	Collections []string `json:"collections"`
}

// ConnectionInfo describes a connected client
type ConnectionInfo struct {
	IP    string  `json:"ip"`
	Since int64   `json:"since"`
	Self  bool    `json:"self"`
	Name  *string `json:"name"`
}

// ConnectionsResponse represents the response for the conn message
type ConnectionsResponse struct {
	ID          string           `json:"id"`
	Connections []ConnectionInfo `json:"connections"`
}

// KeyValueResponse represents a key-value response
type KeyValueResponse struct {
	ID    string `json:"id"`
	Value string `json:"value"`
}

// KeyValueAllKeysResponse represents response with all keys
type KeyValueAllKeysResponse struct {
	ID   string   `json:"id"`
	Keys []string `json:"keys"`
}

// KeyValueAllValuesResponse represents response with all values
type KeyValueAllValuesResponse struct {
	ID     string            `json:"id"`
	Values map[string]string `json:"values"`
}

// CollectionParam represents a collection parameter
type CollectionParam struct {
	Col string `json:"col"`
}

// FetchLatestRecordsParams represents parameters for fetching sessions
type FetchLatestRecordsParams struct {
	Col  string `json:"col"`
	TS   int64  `json:"ts"`
	Doc  string `json:"doc,omitempty"`
	From *int64 `json:"from,omitempty"`
}

// DeleteCollectionParams represents parameters for deleting a collection
type DeleteCollectionParams struct {
	Col string `json:"col"`
}

// FetchRecordsParams represents parameters for fetching records
type FetchRecordsParams struct {
	Col     string `json:"col"`
	Doc     string `json:"doc"`
	From    int64  `json:"from"`
	To      int64  `json:"to"`
	Limit   *int   `json:"limit,omitempty"`
	Reverse *bool  `json:"reverse,omitempty"`
}

// DeleteDocumentParams represents parameters for deleting a document
type DeleteDocumentParams struct {
	Doc string `json:"doc"`
	Col string `json:"col"`
}

// DeleteRecord represents parameters for deleting a record
type DeleteRecord struct {
	Doc string `json:"doc"`
	Col string `json:"col"`
	TS  int64  `json:"ts"`
}

// DeleteRecordsRange represents parameters for deleting records in a timestamp range
type DeleteRecordsRange struct {
	Doc    string `json:"doc"`
	Col    string `json:"col"`
	FromTS int64  `json:"fromTs"`
	ToTS   int64  `json:"toTs"`
}

// SetValueParams represents parameters for setting a value
type SetValueParams struct {
	Col   string `json:"col"`
	Key   string `json:"key"`
	Value string `json:"value"`
}

// GetValueParams represents parameters for getting a value
type GetValueParams struct {
	Col string `json:"col"`
	Key string `json:"key"`
}

// GetValuesParams represents parameters for retrieving values via literal or regex keys
type GetValuesParams struct {
	Col string  `json:"col"`
	Key *string `json:"key,omitempty"`
}

// DeleteValueParams represents parameters for deleting a value
type DeleteValueParams struct {
	Col string `json:"col"`
	Key string `json:"key"`
}

// WebSocketResponse represents a generic WebSocket response
type WebSocketResponse struct {
	ID string `json:"id"`
}

// WebSocketMessage represents a WebSocket message
type WebSocketMessage struct {
	ID   string `json:"id"`
	Type string `json:"type"`
	Data string `json:"data"`
}
