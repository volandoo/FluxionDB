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
  id?: string;
  error?: string;
  [key: string]: unknown;
};

type WebSocketMessage = {
  type: string;
  data: string;
};

type PendingRequest<T = any> = {
  frame: string;
  resolve: (response: T) => void;
  reject: (error: Error) => void;
  sentSocket?: WebSocket | null;
  timeout?: ReturnType<typeof setTimeout>;
};

type ClientOptions = {
  url: string;
  apiKey: string;
  showLogs?: boolean;
  connectionName?: string;
  maxReconnectAttempts?: number;
  reconnectInterval?: number;
  connectTimeout?: number;
  requestTimeout?: number;
};

class Client {
  private ws: WebSocket | null = null;
  private readonly url: string;
  private readonly apiKey: string;
  private readonly maxReconnectAttempts: number;
  private readonly reconnectInterval: number;
  private readonly connectTimeout: number;
  private readonly requestTimeout: number;
  private readonly showLogs: boolean;
  private readonly connectionName?: string;
  private pendingRequests = new Map<string, PendingRequest>();
  private connectionPromise: Promise<void> | null = null;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private reconnectAttempts = 0;
  private shouldReconnect = true;
  private manuallyClosed = false;

  constructor({
    url,
    apiKey,
    showLogs = false,
    connectionName,
    maxReconnectAttempts = Number.POSITIVE_INFINITY,
    reconnectInterval = 5000,
    connectTimeout = 10000,
    requestTimeout = 0,
  }: ClientOptions) {
    this.url = url;
    this.apiKey = apiKey;
    this.showLogs = showLogs;
    this.connectionName = connectionName;
    this.maxReconnectAttempts = maxReconnectAttempts;
    this.reconnectInterval = reconnectInterval;
    this.connectTimeout = connectTimeout;
    this.requestTimeout = requestTimeout;
  }

  public async connect(): Promise<void> {
    if (this.ws?.readyState === WebSocket.OPEN) {
      return;
    }

    if (this.connectionPromise) {
      return this.connectionPromise;
    }

    this.manuallyClosed = false;
    this.shouldReconnect = true;
    this.clearReconnectTimer();

    const promise = this.connectWithRetry();
    this.connectionPromise = promise;
    try {
      await promise;
    } finally {
      if (this.connectionPromise === promise) {
        this.connectionPromise = null;
      }
    }
  }

  public close(): void {
    this.manuallyClosed = true;
    this.shouldReconnect = false;
    this.clearReconnectTimer();

    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }

    this.connectionPromise = null;
    this.rejectPendingRequests(new Error("WebSocket connection closed."));
  }

  private async connectWithRetry(): Promise<void> {
    let failures = 0;

    while (!this.manuallyClosed) {
      try {
        await this.openSocket();
        this.reconnectAttempts = 0;
        this.resendPendingRequests();
        return;
      } catch (error) {
        if (this.manuallyClosed) {
          throw error;
        }

        if (failures >= this.maxReconnectAttempts) {
          throw error;
        }

        failures += 1;
        this.reconnectAttempts = failures;
        const delay = this.reconnectInterval * failures;
        if (this.showLogs) {
          console.warn(
            `WebSocket connection failed: ${(error as Error).message}. Retrying in ${delay}ms... (${failures}/${this.maxReconnectAttempts})`,
          );
        }
        await this.delay(delay);
      }
    }

    throw new Error("WebSocket connection closed.");
  }

  private openSocket(): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      let settled = false;
      let ready = false;
      let connectTimer: ReturnType<typeof setTimeout> | undefined;

      const socket = new WebSocket(this.buildUrl());
      this.ws = socket;

      const cleanup = () => {
        if (connectTimer) {
          clearTimeout(connectTimer);
          connectTimer = undefined;
        }
      };

      const fail = (error: Error) => {
        if (settled) {
          return;
        }
        settled = true;
        cleanup();
        if (this.ws === socket) {
          this.ws = null;
        }
        try {
          socket.close();
        } catch {
          // Ignore close failures while handling a failed connection attempt.
        }
        reject(error);
      };

      const succeed = () => {
        if (settled) {
          return;
        }
        settled = true;
        cleanup();
        resolve();
      };

      connectTimer = setTimeout(() => {
        fail(
          new Error(`WebSocket ready timeout after ${this.connectTimeout}ms.`),
        );
      }, this.connectTimeout);

      socket.addEventListener("open", () => {
        if (this.showLogs) {
          console.log("WebSocket connected, awaiting authentication...");
        }
      });

      socket.addEventListener("message", (event: MessageEvent) => {
        if (this.ws !== socket) {
          return;
        }

        const payload = event.data as string;
        if (!ready) {
          try {
            const message = JSON.parse(payload);
            if (message.type === "ready") {
              ready = true;
              if (this.showLogs) {
                console.log("Authentication successful");
              }
              succeed();
              return;
            }
          } catch (error) {
            fail(error as Error);
            return;
          }
        }

        try {
          this.handleMessage(payload);
        } catch (error) {
          console.error("Error parsing WebSocket message:", error);
        }
      });

      socket.addEventListener("close", (event: CloseEvent) => {
        if (this.ws !== socket) {
          return;
        }

        const reason = event.reason
          ? String(event.reason)
          : `code ${event.code}`;
        if (this.showLogs) {
          console.log("WebSocket disconnected:", event.code, reason);
        }
        this.ws = null;

        if (!ready) {
          fail(new Error(reason));
          return;
        }

        if (this.shouldReconnect && !this.manuallyClosed) {
          this.scheduleReconnect();
        }
      });

      socket.addEventListener("error", (error: any) => {
        if (this.ws !== socket) {
          return;
        }

        const message =
          (error && (error.message || error.toString?.())) ||
          "WebSocket connection error";
        if (!ready) {
          fail(new Error(message));
        } else {
          console.error("WebSocket error:", message, error);
        }
      });
    });
  }

  private handleMessage(payload: string): void {
    const response: WebSocketResponse = JSON.parse(payload);
    if (!response.id) {
      if (response.error) {
        console.warn(`Received error response: ${response.error}`);
      } else {
        console.warn("Received message without id:", response);
      }
      return;
    }

    const request = this.pendingRequests.get(response.id);
    if (request) {
      if (request.timeout) {
        clearTimeout(request.timeout);
      }
      request.resolve(response);
      this.pendingRequests.delete(response.id);
    } else if (response.error) {
      console.warn(`Received error response: ${response.error}`);
    } else {
      console.warn(
        `Received unexpected message with id: ${response.id}`,
        response,
      );
    }
  }

  private async send<T>(data: WebSocketMessage): Promise<T> {
    const messageId = this.generateId();
    const frame = JSON.stringify({
      id: messageId,
      type: data.type,
      data: data.data,
    });

    return new Promise((resolve, reject) => {
      const pending: PendingRequest<T> = {
        frame,
        resolve,
        reject,
      };

      if (this.requestTimeout > 0) {
        pending.timeout = setTimeout(() => {
          this.pendingRequests.delete(messageId);
          reject(
            new Error(
              `Request ${messageId} timed out after ${this.requestTimeout}ms.`,
            ),
          );
        }, this.requestTimeout);
      }

      this.pendingRequests.set(messageId, pending);
      this.connect()
        .then(() => this.sendPendingRequest(messageId, pending))
        .catch((error) => {
          if (pending.timeout) {
            clearTimeout(pending.timeout);
          }
          this.pendingRequests.delete(messageId);
          reject(
            new Error(
              "Failed to connect before sending: " + (error as Error).message,
            ),
          );
        });
    });
  }

  private sendPendingRequest(id: string, request: PendingRequest): void {
    if (!this.pendingRequests.has(id)) {
      return;
    }

    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      this.scheduleReconnect();
      return;
    }

    if (request.sentSocket === this.ws) {
      return;
    }

    try {
      this.ws.send(request.frame);
      request.sentSocket = this.ws;
    } catch (error) {
      if (this.showLogs) {
        console.warn("Error sending WebSocket message:", error);
      }
      this.ws = null;
      this.scheduleReconnect();
    }
  }

  private resendPendingRequests(): void {
    for (const [id, request] of this.pendingRequests) {
      this.sendPendingRequest(id, request);
    }
  }

  private scheduleReconnect(): void {
    if (!this.shouldReconnect || this.manuallyClosed || this.reconnectTimer) {
      return;
    }

    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      this.rejectPendingRequests(
        new Error("WebSocket reconnect attempts exhausted."),
      );
      return;
    }

    this.reconnectAttempts += 1;
    const delay = this.reconnectInterval * this.reconnectAttempts;
    if (this.showLogs) {
      console.warn(
        `Attempting to reconnect... (${this.reconnectAttempts}/${this.maxReconnectAttempts})`,
      );
    }

    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connect().catch((error) => {
        if (this.showLogs) {
          console.warn("Reconnect failed:", error);
        }
        this.scheduleReconnect();
      });
    }, delay);
  }

  private rejectPendingRequests(error: Error): void {
    for (const [id, request] of this.pendingRequests) {
      if (request.timeout) {
        clearTimeout(request.timeout);
      }
      request.reject(error);
      this.pendingRequests.delete(id);
    }
  }

  private clearReconnectTimer(): void {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }

  private buildUrl(): string {
    const params = new URLSearchParams();
    params.set("api-key", this.apiKey);
    if (this.connectionName && this.connectionName.length > 0) {
      params.set("name", this.connectionName);
    }
    const separator = this.url.includes("?") ? "&" : "?";
    return `${this.url}${separator}${params.toString()}`;
  }

  private delay(ms: number): Promise<void> {
    return new Promise((resolve) => {
      setTimeout(resolve, ms);
    });
  }

  private generateId(): string {
    return Math.random().toString(36).substring(2, 15);
  }

  public async fetchCollections() {
    const resp = await this.send<CollectionsResponse>({
      type: MESSAGE_TYPES.QUERY_COLLECTIONS,
      data: "{}",
    });
    return resp.collections;
  }

  public async getConnections(): Promise<ConnectionInfo[]> {
    const resp = await this.send<ConnectionsResponse>({
      type: MESSAGE_TYPES.CONNECTIONS,
      data: "{}",
    });
    return resp.connections;
  }

  public async deleteCollection(params: DeleteCollectionParams) {
    await this.send<InsertMessageResponse>({
      type: MESSAGE_TYPES.DELETE_COLLECTION,
      data: JSON.stringify(params),
    });
  }

  /**
   * Fetch the latest records for a given collection. If device_id is provided, fetch the latest records for that device_id only
   * if "from" is provided, fetch the records only starting from that timestamp and always up to the "ts" timestamp.
   */
  public async fetchLatestRecords(params: FetchLatestRecordsParams) {
    const resp = await this.send<QueryResponse>({
      type: MESSAGE_TYPES.QUERY_RECORDS,
      data: JSON.stringify({
        col: params.col,
        ts: params.ts,
        doc: params.doc || "",
        from: params.from || 0,
        where: params.where,
        filter: params.filter,
      }),
    });
    return resp.records;
  }

  public async insertMultipleRecords(items: InsertMessageRequest[]) {
    return this.send({
      data: JSON.stringify(items),
      type: MESSAGE_TYPES.INSERT,
    });
  }

  public async insertSingleRecord(items: InsertMessageRequest) {
    return this.send({
      data: JSON.stringify([items]),
      type: MESSAGE_TYPES.INSERT,
    });
  }

  public async fetchDocument(params: FetchRecordsParams) {
    const resp = await this.send<QueryCollectionResponse>({
      type: MESSAGE_TYPES.QUERY_DOCUMENT,
      data: JSON.stringify(params),
    });
    return resp.records;
  }

  public async deleteDocument(params: DeleteDocumentParams) {
    await this.send<InsertMessageResponse>({
      type: MESSAGE_TYPES.DELETE_DOCUMENT,
      data: JSON.stringify(params),
    });
  }

  public async deleteRecord(params: DeleteRecord) {
    await this.send<InsertMessageResponse>({
      type: MESSAGE_TYPES.DELETE_RECORD,
      data: JSON.stringify(params),
    });
  }

  public async deleteMultipleRecords(params: DeleteRecord[]) {
    await this.send<InsertMessageResponse>({
      type: MESSAGE_TYPES.DELETE_MULTIPLE_RECORDS,
      data: JSON.stringify(params),
    });
  }

  public async deleteRecordsRange(params: DeleteRecordsRange) {
    await this.send<InsertMessageResponse>({
      type: MESSAGE_TYPES.DELETE_RECORDS_RANGE,
      data: JSON.stringify(params),
    });
  }

  // Key Value API
  public async setValue(params: SetValueParams) {
    await this.send<InsertMessageResponse>({
      type: MESSAGE_TYPES.SET_VALUE,
      data: JSON.stringify(params),
    });
  }

  public async getValue(params: GetValueParams) {
    const resp = await this.send<KeyValueResponse>({
      type: MESSAGE_TYPES.GET_VALUE,
      data: JSON.stringify(params),
    });
    return resp.value;
  }

  public async getKeys(params: CollectionParam) {
    const resp = await this.send<KeyValueAllKeysResponse>({
      type: MESSAGE_TYPES.GET_ALL_KEYS,
      data: JSON.stringify(params),
    });
    return resp.keys;
  }

  public async getValues(params: GetValuesParams) {
    const type =
      params.key && params.key.length > 0
        ? MESSAGE_TYPES.GET_VALUES
        : MESSAGE_TYPES.GET_ALL_VALUES;
    const resp = await this.send<KeyValueAllValuesResponse>({
      type,
      data: JSON.stringify(params),
    });
    return resp.values;
  }

  public async deleteValue(params: DeleteValueParams) {
    await this.send<InsertMessageResponse>({
      type: MESSAGE_TYPES.REMOVE_VALUE,
      data: JSON.stringify(params),
    });
  }

  public async addApiKey(
    params: AddApiKeyParams,
  ): Promise<ManageApiKeyResponse> {
    return this.manageApiKey({
      action: "add",
      key: params.key,
      scope: params.scope,
    });
  }

  public async removeApiKey(
    params: RemoveApiKeyParams,
  ): Promise<ManageApiKeyResponse> {
    return this.manageApiKey({
      action: "remove",
      key: params.key,
    });
  }

  private async manageApiKey(
    params: ManageApiKeyParams,
  ): Promise<ManageApiKeyResponse> {
    return this.send<ManageApiKeyResponse>({
      type: MESSAGE_TYPES.MANAGE_API_KEYS,
      data: JSON.stringify(params),
    });
  }
}

export default Client;
