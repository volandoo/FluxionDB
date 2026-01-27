import {
  CollectionsResponse,
  FetchLatestRecordsParams,
  QueryResponse,
  DeleteCollectionParams,
  InsertMessageResponse,
  QueryCollectionResponse,
  DeleteRecord,
  DeleteRecordsRange,
  SetValueParams,
  GetValueParams,
  GetValuesParams,
  KeyValueResponse,
  KeyValueAllKeysResponse,
  KeyValueAllValuesResponse,
  ConnectionInfo,
  ConnectionsResponse,
  CollectionParam,
  DeleteValueParams,
  InsertMessageRequest,
  FetchRecordsParams,
  DeleteDocumentParams,
  ManageApiKeyResponse,
  AddApiKeyParams,
  RemoveApiKeyParams,
  ManageApiKeyParams,
} from "./types";
import TimeoutError from "./errors";

// Lazy load WebSocket implementation

const MESSAGE_TYPES = {
  INSERT: "ins",
  QUERY_RECORDS: "qry",
  QUERY_COLLECTIONS: "cols",
  QUERY_DOCUMENT: "qdoc",
  DELETE_DOCUMENT: "ddoc",
  DELETE_COLLECTION: "dcol",
  DELETE_RECORD: "drec",
  DELETE_MULTIPLE_RECORDS: "dmrec",
  DELETE_RECORDS_RANGE: "drrng",
  SET_VALUE: "sval",
  GET_VALUE: "gval",
  GET_VALUES: "gvalues",
  REMOVE_VALUE: "rval",
  GET_ALL_VALUES: "gvals",
  GET_ALL_KEYS: "gkeys",
  MANAGE_API_KEYS: "keys",
  CONNECTIONS: "conn",
} as const;

type WebSocketResponse = {
  id: string;
  [key: string]: string;
};

type WebSocketMessage = {
  type: string;
  data: string;
};

type ClientOptions = {
  url: string;
  apiKey: string;
  showLogs?: boolean;
  connectionName?: string;
  maxReconnectAttempts?: number;
  reconnectInterval?: number;
};

class Client {
  private ws: WebSocket | null = null;
  private readonly url: string;
  private inflightRequests: { [id: string]: (response: any) => void; } = {};
  private isConnecting: boolean = false; // Track if we're currently attempting to connect
  private isReconnecting: boolean = false; // Flag to prevent concurrent reconnect attempts
  private reconnectAttempts: number = 0;
  private readonly maxReconnectAttempts: number;
  private readonly reconnectInterval: number; // milliseconds
  private connectionPromise: Promise<void> | null = null; // Promise to track connection status
  private apiKey: string;
  private shouldReconnect: boolean = true; // Flag to track if reconnection should happen
  private showLogs: boolean = false;
  private connectionName?: string;

  constructor({
    url,
    apiKey,
    showLogs = false,
    connectionName,
    maxReconnectAttempts = 5,
    reconnectInterval = 5000,
  }: ClientOptions) {
    this.url = url;
    this.apiKey = apiKey;
    this.showLogs = showLogs;
    this.connectionName = connectionName;
    this.maxReconnectAttempts = maxReconnectAttempts;
    this.reconnectInterval = reconnectInterval;
  }

  public async connect(): Promise<void> {
    // Load WebSocket implementation first

    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      return;
    }

    if (this.isConnecting) {
      console.warn("WebSocket connection already in progress.");
      return this.connectionPromise!; // Return existing promise
    }

    if (this.connectionPromise) {
      return this.connectionPromise; // Return existing promise
    }

    this.isConnecting = true;
    this.connectionPromise = new Promise<void>((resolve, reject) => {
      let isAuthenticated = false; // Track if we received the "ready" message from server
      let hasSettled = false; // Track if the promise has resolved or rejected
      let handledFailure = false; // Prevent handling the same failure twice (error + close)

      const handleInitialFailure = (errorMessage: string) => {
        if (handledFailure) {
          return;
        }
        handledFailure = true;
        this.isConnecting = false;
        this.connectionPromise = null;
        this.ws = null;
        this.cleanupOnClose();

        if (this.reconnectAttempts < this.maxReconnectAttempts && this.shouldReconnect) {
          const attempt = ++this.reconnectAttempts;
          const delay = this.reconnectInterval * attempt;
          if (this.showLogs) {
            console.warn(
              `Initial connection failed (${errorMessage}). Retrying in ${delay}ms... (${attempt}/${this.maxReconnectAttempts})`,
            );
          }

          setTimeout(() => {
            handledFailure = false; // Allow the next attempt to process its own failure
            this.connect()
              .then(resolve)
              .catch(reject);
          }, delay);
        } else if (!hasSettled) {
          hasSettled = true;
          reject(new Error(errorMessage));
        }
      };

      try {
        // Append API key as query parameter instead of header
        // (Qt 6.4 doesn't support reading custom headers from handshake)
        const params = new URLSearchParams();
        params.set("api-key", this.apiKey);
        if (this.connectionName && this.connectionName.length > 0) {
          params.set("name", this.connectionName);
        }
        const urlWithKey = `${this.url}${this.url.includes("?") ? "&" : "?"}${params.toString()}`;
        if (this.showLogs) {
          console.log("Connecting to:", urlWithKey);
        }
        this.ws = new WebSocket(urlWithKey);
        if (this.showLogs) {
          console.log("WebSocket created, readyState:", this.ws);
        }
      } catch (error) {
        this.isConnecting = false;
        this.connectionPromise = null;
        reject(error as Error);
        return;
      }

      // Handle open event - don't resolve yet, wait for "ready" message
      const onOpen = () => {
        if (this.showLogs) {
          console.log("WebSocket connected, awaiting authentication...");
        }
        // Don't resolve here - wait for server to send "ready" message
      };

      // Handle message event
      const onMessage = (event: MessageEvent) => {
        try {
          const payload = event.data as string;

          if (payload !== null) {
            // Check if this is the "ready" message from server during authentication
            if (!isAuthenticated && this.isConnecting) {
              if (this.showLogs) {
                console.log(
                  "Received message during authentication:",
                  payload.substring(0, 100),
                );
              }
              try {
                const msg = JSON.parse(payload);
                if (this.showLogs) {
                  console.log("Parsed message type:", msg.type);
                }
                if (msg.type === "ready") {
                  if (this.showLogs) {
                    console.log("Authentication successful");
                  }
                  isAuthenticated = true;
                  hasSettled = true;
                  this.isConnecting = false;
                  this.reconnectAttempts = 0;
                  this.isReconnecting = false;
                  this.shouldReconnect = true;
                  resolve();
                  return; // Don't pass ready message to handleMessage
                }
              } catch (e) {
                if (this.showLogs) {
                  console.log("Failed to parse message during auth:", e);
                }
                // Not JSON or not ready message, handle normally
              }
            }
            this.handleMessage(payload);
          }
        } catch (error) {
          console.error("Error parsing WebSocket message:", error);
        }
      };

      // Handle close event
      const onClose = (event: CloseEvent) => {
        const code = event.code;
        const reason = event.reason ? String(event.reason) : "";
        const reasonText = reason || "no reason";
        if (this.showLogs) {
          console.log("WebSocket disconnected:", code, reasonText);
        }
        this.isConnecting = false;
        this.connectionPromise = null; // Clear the promise on close/error
        this.ws = null;
        this.cleanupOnClose();

        // If closed before authentication, it's an auth failure
        if (!isAuthenticated) {
          handleInitialFailure(reasonText || `code ${code}`);
        } else if (this.shouldReconnect) {
          this.reconnect();
        }
      };

      // Handle error event
      const onError = (error: any) => {
        const errorMessage =
          (error && (error.message || error.toString?.())) ||
          "WebSocket connection error";
        console.error("WebSocket error:", errorMessage, error);
        if (!isAuthenticated) {
          handleInitialFailure(errorMessage);
        } else if (this.shouldReconnect) {
          this.isConnecting = false;
          this.connectionPromise = null; // Clear the promise on close/error
          this.ws = null;
          this.cleanupOnClose();
          this.reconnect(); // Treat errors like a close event and attempt reconnect
        }
      };

      // Attach event listeners based on environment
      if (!this.ws) {
        reject(new Error("Failed to create WebSocket"));
        return;
      }

      this.ws.addEventListener("open", onOpen);
      this.ws.addEventListener("message", onMessage);
      this.ws.addEventListener("close", onClose);
      this.ws.addEventListener("error", onError);
    });

    return this.connectionPromise;
  }

  private handleMessage(payload: string): void {
    const response: WebSocketResponse = JSON.parse(payload);
    const callback = this.inflightRequests[response.id];
    if (callback) {
      callback(response);
      delete this.inflightRequests[response.id];
    } else if (response.error) {
      console.warn(`Received error response: ${response.error}`);
    } else {
      console.warn(
        `Received unexpected message with id: ${response.id}`,
        response,
      );
    }
  }

  private cleanupOnClose(): void {
    // Clean up inflight requests
    for (const id in this.inflightRequests) {
      if (this.inflightRequests.hasOwnProperty(id)) {
        console.warn(`Request ${id} timed out due to disconnection.`);
        // You could also reject the promises here if using them,
        //  or provide an error handling strategy.
        delete this.inflightRequests[id];
      }
    }
  }

  private startTimeout(onTimeout: () => void, timeout?: number): () => void {
    if (timeout === undefined) {
      return () => {};
    }
    const timer = setTimeout(onTimeout, timeout);
    return () => clearTimeout(timer);
  }

  private async send<T>(
    data: WebSocketMessage,
    timeout?: number,
  ): Promise<T> {
    try {
      await this.connect();
    } catch (error) {
      return Promise.reject(
        new Error(
          "Failed to connect before sending: " + (error as Error).message,
        ),
      ); // Reject send if connect fails.
    }

    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      return Promise.reject(new Error("WebSocket not connected."));
    }

    const messageId = this.generateId();

    return new Promise((resolve, reject) => {
      let settled = false;
      const clearTimer = this.startTimeout(() => {
        if (settled) {
          return;
        }
        settled = true;
        delete this.inflightRequests[messageId];
        reject(
          new TimeoutError(
            `Request ${messageId} timed out after ${timeout}ms`,
          ),
        );
      }, timeout);

      const resolveOnce = (response: T) => {
        if (settled) {
          return;
        }
        settled = true;
        clearTimer();
        delete this.inflightRequests[messageId];
        resolve(response);
      };

      const rejectOnce = (error: any) => {
        if (settled) {
          return;
        }
        settled = true;
        clearTimer();
        delete this.inflightRequests[messageId];
        reject(error);
      };

      this.inflightRequests[messageId] = resolveOnce;
      try {
        this.ws!.send(
          JSON.stringify({
            id: messageId,
            type: data.type,
            data: data.data,
          }),
        );
      } catch (error) {
        console.error("Error sending message:", error);
        rejectOnce(error);
      }
    });
  }

  private generateId(): string {
    return Math.random().toString(36).substring(2, 15);
  }

  public close(): void {
    this.shouldReconnect = false; // Prevent automatic reconnection
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
    this.cleanupOnClose();
    if (this.connectionPromise) {
      // If the connection promise exists, we'll reject it.
      this.connectionPromise = null;
    }
  }

  private reconnect(): void {
    if (
      this.isReconnecting ||
      this.reconnectAttempts >= this.maxReconnectAttempts
    ) {
      return; // Prevent concurrent reconnects and stop if max attempts reached
    }

    this.isReconnecting = true;
    this.reconnectAttempts++;

    if (this.reconnectAttempts > 1) {
      console.warn(
        `Attempting to reconnect... (Attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts})`,
      );
    }
    setTimeout(() => {
      this.connect();
      this.isReconnecting = false;
    }, this.reconnectInterval);
  }

  public async fetchCollections(timeout?: number) {
    const resp = await this.send<CollectionsResponse>(
      {
        type: MESSAGE_TYPES.QUERY_COLLECTIONS,
        data: "{}",
      },
      timeout,
    );
    return resp.collections;
  }

  public async getConnections(
    timeout?: number,
  ): Promise<ConnectionInfo[]> {
    const resp = await this.send<ConnectionsResponse>(
      {
        type: MESSAGE_TYPES.CONNECTIONS,
        data: "{}",
      },
      timeout,
    );
    return resp.connections;
  }

  public async deleteCollection(
    params: DeleteCollectionParams,
    timeout?: number,
  ) {
    await this.send<InsertMessageResponse>(
      {
        type: MESSAGE_TYPES.DELETE_COLLECTION,
        data: JSON.stringify(params),
      },
      timeout,
    );
  }

  /**
   * Fetch the latest records for a given collection. If device_id is provided, fetch the latest records for that device_id only
   * if "from" is provided, fetch the records only starting from that timestamp and always up to the "ts" timestamp.
   */
  public async fetchLatestRecords(
    params: FetchLatestRecordsParams,
    timeout?: number,
  ) {
    const resp = await this.send<QueryResponse>(
      {
        type: MESSAGE_TYPES.QUERY_RECORDS,
        data: JSON.stringify({
          col: params.col,
          ts: params.ts,
          doc: params.doc || "",
          from: params.from || 0,
        }),
      },
      timeout,
    );
    return resp.records;
  }

  public async insertMultipleRecords(
    items: InsertMessageRequest[],
    timeout?: number,
  ) {
    return this.send(
      {
        data: JSON.stringify(items),
        type: MESSAGE_TYPES.INSERT,
      },
      timeout,
    );
  }

  public async insertSingleRecord(
    items: InsertMessageRequest,
    timeout?: number,
  ) {
    return this.send(
      {
        data: JSON.stringify([items]),
        type: MESSAGE_TYPES.INSERT,
      },
      timeout,
    );
  }

  public async fetchDocument(
    params: FetchRecordsParams,
    timeout?: number,
  ) {
    const resp = await this.send<QueryCollectionResponse>(
      {
        type: MESSAGE_TYPES.QUERY_DOCUMENT,
        data: JSON.stringify(params),
      },
      timeout,
    );
    return resp.records;
  }

  public async deleteDocument(
    params: DeleteDocumentParams,
    timeout?: number,
  ) {
    await this.send<InsertMessageResponse>(
      {
        type: MESSAGE_TYPES.DELETE_DOCUMENT,
        data: JSON.stringify(params),
      },
      timeout,
    );
  }

  public async deleteRecord(params: DeleteRecord, timeout?: number) {
    await this.send<InsertMessageResponse>(
      {
        type: MESSAGE_TYPES.DELETE_RECORD,
        data: JSON.stringify(params),
      },
      timeout,
    );
  }

  public async deleteMultipleRecords(
    params: DeleteRecord[],
    timeout?: number,
  ) {
    await this.send<InsertMessageResponse>(
      {
        type: MESSAGE_TYPES.DELETE_MULTIPLE_RECORDS,
        data: JSON.stringify(params),
      },
      timeout,
    );
  }

  public async deleteRecordsRange(
    params: DeleteRecordsRange,
    timeout?: number,
  ) {
    await this.send<InsertMessageResponse>(
      {
        type: MESSAGE_TYPES.DELETE_RECORDS_RANGE,
        data: JSON.stringify(params),
      },
      timeout,
    );
  }

  // Key Value API
  public async setValue(params: SetValueParams, timeout?: number) {
    await this.send<InsertMessageResponse>(
      {
        type: MESSAGE_TYPES.SET_VALUE,
        data: JSON.stringify(params),
      },
      timeout,
    );
  }

  public async getValue(params: GetValueParams, timeout?: number) {
    const resp = await this.send<KeyValueResponse>(
      {
        type: MESSAGE_TYPES.GET_VALUE,
        data: JSON.stringify(params),
      },
      timeout,
    );
    return resp.value;
  }

  public async getKeys(params: CollectionParam, timeout?: number) {
    const resp = await this.send<KeyValueAllKeysResponse>(
      {
        type: MESSAGE_TYPES.GET_ALL_KEYS,
        data: JSON.stringify(params),
      },
      timeout,
    );
    return resp.keys;
  }

  public async getValues(params: GetValuesParams, timeout?: number) {
    const type =
      params.key && params.key.length > 0
        ? MESSAGE_TYPES.GET_VALUES
        : MESSAGE_TYPES.GET_ALL_VALUES;
    const resp = await this.send<KeyValueAllValuesResponse>(
      {
        type,
        data: JSON.stringify(params),
      },
      timeout,
    );
    return resp.values;
  }

  public async deleteValue(params: DeleteValueParams, timeout?: number) {
    await this.send<InsertMessageResponse>(
      {
        type: MESSAGE_TYPES.REMOVE_VALUE,
        data: JSON.stringify(params),
      },
      timeout,
    );
  }

  public async addApiKey(
    params: AddApiKeyParams,
    timeout?: number,
  ): Promise<ManageApiKeyResponse> {
    return this.manageApiKey(
      {
        action: "add",
        key: params.key,
        scope: params.scope,
      },
      timeout,
    );
  }

  public async removeApiKey(
    params: RemoveApiKeyParams,
    timeout?: number,
  ): Promise<ManageApiKeyResponse> {
    return this.manageApiKey(
      {
        action: "remove",
        key: params.key,
      },
      timeout,
    );
  }

  private async manageApiKey(
    params: ManageApiKeyParams,
    timeout?: number,
  ): Promise<ManageApiKeyResponse> {
    return this.send<ManageApiKeyResponse>(
      {
        type: MESSAGE_TYPES.MANAGE_API_KEYS,
        data: JSON.stringify(params),
      },
      timeout,
    );
  }
}

export default Client;
