package fluxiondb

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"math/rand"
	"net/url"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

const (
	messageTypeInsert           = "ins"
	messageTypeQueryRecords     = "qry"
	messageTypeQueryCollections = "cols"
	messageTypeQueryDocument    = "qdoc"
	messageTypeDeleteDocument   = "ddoc"
	messageTypeDeleteCollection = "dcol"
	messageTypeDeleteRecord     = "drec"
	messageTypeDeleteMultiple   = "dmrec"
	messageTypeDeleteRange      = "drrng"
	messageTypeSetValue         = "sval"
	messageTypeGetValue         = "gval"
	messageTypeGetValues        = "gvalues"
	messageTypeRemoveValue      = "rval"
	messageTypeGetAllValues     = "gvals"
	messageTypeGetAllKeys       = "gkeys"
	messageTypeManageKeys       = "keys"
	messageTypeConnections      = "conn"
)

// Client represents an  WebSocket client
type Client struct {
	url                  string
	apiKey               string
	connectionName       string
	conn                 *websocket.Conn
	inflightRequests     map[string]chan json.RawMessage
	inflightMutex        sync.RWMutex
	isConnecting         bool
	isReconnecting       bool
	reconnectAttempts    int
	maxReconnectAttempts int
	reconnectInterval    time.Duration
	connectionMutex      sync.Mutex
	ctx                  context.Context
	cancel               context.CancelFunc
}

// NewClient creates a new  client
func NewClient(wsURL, apiKey string) *Client {
	ctx, cancel := context.WithCancel(context.Background())
	return &Client{
		url:                  wsURL,
		apiKey:               apiKey,
		inflightRequests:     make(map[string]chan json.RawMessage),
		maxReconnectAttempts: 5,
		reconnectInterval:    5 * time.Second,
		ctx:                  ctx,
		cancel:               cancel,
	}
}

// NewClientWithName creates a new client and assigns a connection label
func NewClientWithName(wsURL, apiKey, connectionName string) *Client {
	client := NewClient(wsURL, apiKey)
	client.connectionName = connectionName
	return client
}

// SetConnectionName configures the name query parameter sent when connecting
func (c *Client) SetConnectionName(name string) {
	c.connectionName = name
}

// Connect establishes a WebSocket connection to the  server
func (c *Client) Connect() error {
	c.connectionMutex.Lock()
	defer c.connectionMutex.Unlock()

	if c.conn != nil {
		return nil
	}

	if c.isConnecting {
		return errors.New("connection already in progress")
	}

	c.isConnecting = true
	defer func() { c.isConnecting = false }()

	u, err := url.Parse(c.url)
	if err != nil {
		return fmt.Errorf("invalid URL: %w", err)
	}

	// Append API key (and optional connection name) as query parameters
	// (Qt 6.4 doesn't support reading custom headers from handshake)
	q := u.Query()
	q.Set("api-key", c.apiKey)
	if c.connectionName != "" {
		q.Set("name", c.connectionName)
	}
	u.RawQuery = q.Encode()

	dialer := websocket.DefaultDialer
	conn, _, err := dialer.Dial(u.String(), nil)
	if err != nil {
		return fmt.Errorf("failed to connect: %w", err)
	}

	log.Println("WebSocket connected, awaiting authentication...")

	// Wait for the "ready" message from server to confirm authentication
	var response map[string]interface{}
	conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	err = conn.ReadJSON(&response)
	conn.SetReadDeadline(time.Time{}) // Clear deadline
	if err != nil {
		conn.Close()
		return fmt.Errorf("authentication failed: %w", err)
	}

	msgType, ok := response["type"].(string)
	if !ok || msgType != "ready" {
		conn.Close()
		return fmt.Errorf("authentication failed: expected ready message, got %v", response)
	}

	log.Println("Authentication successful")

	c.conn = conn
	c.reconnectAttempts = 0
	c.isReconnecting = false

	go c.readMessages()

	return nil
}

// readMessages handles incoming WebSocket messages
func (c *Client) readMessages() {
	defer c.cleanupConnection()

	for {
		select {
		case <-c.ctx.Done():
			return
		default:
		}

		// Check if connection is still valid before reading
		if c.conn == nil {
			return
		}

		var response map[string]interface{}
		err := c.conn.ReadJSON(&response)
		if err != nil {
			// Only log and reconnect if context is not cancelled
			select {
			case <-c.ctx.Done():
				return
			default:
				log.Printf("Error reading message: %v", err)
				c.reconnect()
				return
			}
		}

		id, ok := response["id"].(string)
		if !ok {
			log.Printf("Received message without ID: %v", response)
			continue
		}

		c.inflightMutex.RLock()
		responseChan, exists := c.inflightRequests[id]
		c.inflightMutex.RUnlock()

		if exists {
			responseBytes, _ := json.Marshal(response)
			select {
			case responseChan <- responseBytes:
			default:
				log.Printf("Failed to send response for ID %s", id)
			}

			c.inflightMutex.Lock()
			delete(c.inflightRequests, id)
			c.inflightMutex.Unlock()
		} else {
			log.Printf("Received unexpected message with ID: %s", id)
		}
	}
}

// cleanupConnection cleans up the connection and inflight requests
func (c *Client) cleanupConnection() {
	c.connectionMutex.Lock()
	defer c.connectionMutex.Unlock()

	if c.conn != nil {
		c.conn.Close()
		c.conn = nil
	}

	c.inflightMutex.Lock()
	for id, ch := range c.inflightRequests {
		select {
		case <-ch:
			// Channel already closed
		default:
			close(ch)
		}
		delete(c.inflightRequests, id)
	}
	c.inflightMutex.Unlock()
}

// reconnect attempts to reconnect to the server
func (c *Client) reconnect() {
	if c.isReconnecting || c.reconnectAttempts >= c.maxReconnectAttempts {
		return
	}

	c.isReconnecting = true
	c.reconnectAttempts++

	log.Printf("Attempting to reconnect... (Attempt %d/%d)", c.reconnectAttempts, c.maxReconnectAttempts)

	time.Sleep(c.reconnectInterval)

	c.connectionMutex.Lock()
	c.isConnecting = false
	c.connectionMutex.Unlock()

	err := c.Connect()
	if err != nil {
		log.Printf("Reconnection failed: %v", err)
	}

	c.isReconnecting = false
}

// send sends a message and waits for a response
func (c *Client) send(msgType, data string, response interface{}) error {
	if c.conn == nil {
		if err := c.Connect(); err != nil {
			return fmt.Errorf("failed to connect: %w", err)
		}
	}

	messageID := c.generateID()
	responseChan := make(chan json.RawMessage, 1)

	c.inflightMutex.Lock()
	c.inflightRequests[messageID] = responseChan
	c.inflightMutex.Unlock()

	msg := WebSocketMessage{
		ID:   messageID,
		Type: msgType,
		Data: data,
	}

	if err := c.conn.WriteJSON(msg); err != nil {
		c.inflightMutex.Lock()
		delete(c.inflightRequests, messageID)
		c.inflightMutex.Unlock()
		return fmt.Errorf("failed to send message: %w", err)
	}

	select {
	case responseData := <-responseChan:
		return json.Unmarshal(responseData, response)
	case <-time.After(30 * time.Second):
		c.inflightMutex.Lock()
		delete(c.inflightRequests, messageID)
		c.inflightMutex.Unlock()
		return errors.New("request timeout")
	case <-c.ctx.Done():
		c.inflightMutex.Lock()
		delete(c.inflightRequests, messageID)
		c.inflightMutex.Unlock()
		return c.ctx.Err()
	}
}

// generateID generates a random ID for messages
func (c *Client) generateID() string {
	const charset = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 13)
	for i := range b {
		b[i] = charset[rand.Intn(len(charset))]
	}
	return string(b)
}

// Close closes the client connection
func (c *Client) Close() {
	c.cancel()

	// Wait a moment for the readMessages goroutine to exit
	time.Sleep(100 * time.Millisecond)

	c.cleanupConnection()
}

// FetchCollections fetches all available collections
func (c *Client) FetchCollections() ([]string, error) {
	var response CollectionsResponse
	err := c.send(messageTypeQueryCollections, "{}", &response)
	if err != nil {
		return nil, err
	}
	return response.Collections, nil
}

// GetConnections returns metadata about active connections
func (c *Client) GetConnections() ([]ConnectionInfo, error) {
	var response ConnectionsResponse
	if err := c.send(messageTypeConnections, "{}", &response); err != nil {
		return nil, err
	}
	return response.Connections, nil
}

// FetchLatestRecords fetches the latest records per document for a collection
func (c *Client) FetchLatestRecords(params FetchLatestRecordsParams) (map[string]RecordResponse, error) {
	data, err := json.Marshal(params)
	if err != nil {
		return nil, err
	}

	var response QueryResponse
	err = c.send(messageTypeQueryRecords, string(data), &response)
	if err != nil {
		return nil, err
	}
	return response.Records, nil
}

// DeleteCollection deletes a collection
func (c *Client) DeleteCollection(params DeleteCollectionParams) error {
	data, err := json.Marshal(params)
	if err != nil {
		return err
	}

	var response InsertMessageResponse
	return c.send(messageTypeDeleteCollection, string(data), &response)
}

// InsertMultipleRecords inserts multiple records into a document
func (c *Client) InsertMultipleRecords(items []InsertMessageRequest) error {
	data, err := json.Marshal(items)
	if err != nil {
		return err
	}

	var response InsertMessageResponse
	return c.send(messageTypeInsert, string(data), &response)
}

// InsertSingleDocumentRecord inserts a single record into a document
func (c *Client) InsertSingleDocumentRecord(item InsertMessageRequest) error {
	return c.InsertMultipleRecords([]InsertMessageRequest{item})
}

// FetchDocument fetches all records for a specific document according to the provided filters
func (c *Client) FetchDocument(params FetchRecordsParams) ([]RecordResponse, error) {
	data, err := json.Marshal(params)
	if err != nil {
		return nil, err
	}

	var response QueryCollectionResponse
	err = c.send(messageTypeQueryDocument, string(data), &response)
	if err != nil {
		return nil, err
	}
	return response.Records, nil
}

// DeleteDocument deletes an entire document (optionally scoped to a collection)
func (c *Client) DeleteDocument(params DeleteDocumentParams) error {
	data, err := json.Marshal(params)
	if err != nil {
		return err
	}

	var response InsertMessageResponse
	return c.send(messageTypeDeleteDocument, string(data), &response)
}

// DeleteDocumentRecord deletes a single record within a document
func (c *Client) DeleteDocumentRecord(params DeleteRecord) error {
	data, err := json.Marshal(params)
	if err != nil {
		return err
	}

	var response InsertMessageResponse
	return c.send(messageTypeDeleteRecord, string(data), &response)
}

// DeleteMultipleRecords deletes multiple records across one or more documents
func (c *Client) DeleteMultipleRecords(params []DeleteRecord) error {
	data, err := json.Marshal(params)
	if err != nil {
		return err
	}

	var response InsertMessageResponse
	return c.send(messageTypeDeleteMultiple, string(data), &response)
}

// DeleteRecordsRange deletes records within a timestamp range for a specific document
func (c *Client) DeleteRecordsRange(params DeleteRecordsRange) error {
	data, err := json.Marshal(params)
	if err != nil {
		return err
	}

	var response InsertMessageResponse
	return c.send(messageTypeDeleteRange, string(data), &response)
}

// SetValue sets a key-value pair
func (c *Client) SetValue(params SetValueParams) error {
	data, err := json.Marshal(params)
	if err != nil {
		return err
	}

	var response InsertMessageResponse
	return c.send(messageTypeSetValue, string(data), &response)
}

// GetValue gets a value by key
func (c *Client) GetValue(params GetValueParams) (string, error) {
	data, err := json.Marshal(params)
	if err != nil {
		return "", err
	}

	var response KeyValueResponse
	err = c.send(messageTypeGetValue, string(data), &response)
	if err != nil {
		return "", err
	}
	return response.Value, nil
}

// GetKeys gets all keys in a collection
func (c *Client) GetKeys(params CollectionParam) ([]string, error) {
	data, err := json.Marshal(params)
	if err != nil {
		return nil, err
	}

	var response KeyValueAllKeysResponse
	err = c.send(messageTypeGetAllKeys, string(data), &response)
	if err != nil {
		return nil, err
	}
	return response.Keys, nil
}

// GetValues fetches values for an entire collection or filters by optional key/regex
func (c *Client) GetValues(params GetValuesParams) (map[string]string, error) {
	data, err := json.Marshal(params)
	if err != nil {
		return nil, err
	}
	var response KeyValueAllValuesResponse
	msgType := messageTypeGetAllValues
	if params.Key != nil && *params.Key != "" {
		msgType = messageTypeGetValues
	}
	if err = c.send(msgType, string(data), &response); err != nil {
		return nil, err
	}
	return response.Values, nil
}

// DeleteValue deletes a key-value pair
func (c *Client) DeleteValue(params DeleteValueParams) error {
	data, err := json.Marshal(params)
	if err != nil {
		return err
	}

	var response InsertMessageResponse
	return c.send(messageTypeRemoveValue, string(data), &response)
}

// AddAPIKey registers a new API key with the specified scope
func (c *Client) AddAPIKey(key string, scope ApiKeyScope) (ManageAPIKeyResponse, error) {
	payload := ManageAPIKeyParams{
		Action: "add",
		Key:    key,
		Scope:  scope,
	}

	return c.manageAPIKey(payload)
}

// RemoveAPIKey deletes a previously created API key
func (c *Client) RemoveAPIKey(key string) (ManageAPIKeyResponse, error) {
	payload := ManageAPIKeyParams{
		Action: "remove",
		Key:    key,
	}

	return c.manageAPIKey(payload)
}

// ListAPIKeys fetches all registered API keys (master key required)
func (c *Client) ListAPIKeys() ([]APIKeyInfo, error) {
	payload := ManageAPIKeyParams{
		Action: "list",
	}
	data, err := json.Marshal(payload)
	if err != nil {
		return nil, err
	}

	var response ManageAPIKeyResponse
	if err := c.send(messageTypeManageKeys, string(data), &response); err != nil {
		return nil, err
	}
	if response.Error != "" {
		return nil, errors.New(response.Error)
	}
	if response.Keys == nil {
		return []APIKeyInfo{}, nil
	}
	return response.Keys, nil
}

func (c *Client) manageAPIKey(params ManageAPIKeyParams) (ManageAPIKeyResponse, error) {
	data, err := json.Marshal(params)
	if err != nil {
		return ManageAPIKeyResponse{}, err
	}

	var response ManageAPIKeyResponse
	if err := c.send(messageTypeManageKeys, string(data), &response); err != nil {
		return ManageAPIKeyResponse{}, err
	}

	return response, nil
}
