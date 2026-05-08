import { describe, it, expect, afterEach } from "bun:test";
import FluxionDBClient from "./client";

class MockWebSocket {
  static readonly CONNECTING = 0;
  static readonly OPEN = 1;
  static readonly CLOSING = 2;
  static readonly CLOSED = 3;

  public readyState = MockWebSocket.CONNECTING;
  private listeners: Record<
    "open" | "message" | "close" | "error",
    Array<(event: any) => void>
  > = {
    open: [],
    message: [],
    close: [],
    error: [],
  };

  constructor(public readonly url: string) {}
  public readonly sentFrames: string[] = [];

  public addEventListener(type: keyof typeof this.listeners, listener: any) {
    this.listeners[type].push(listener);
  }

  public removeEventListener(type: keyof typeof this.listeners, listener: any) {
    this.listeners[type] = this.listeners[type].filter((l) => l !== listener);
  }

  public send(frame: string) {
    this.sentFrames.push(frame);
  }

  public close(code = 1000, reason = "") {
    this.readyState = MockWebSocket.CLOSED;
    this.dispatch("close", { code, reason });
  }

  public emitOpen() {
    this.readyState = MockWebSocket.OPEN;
    this.dispatch("open", {});
  }

  public emitMessage(data: any) {
    this.dispatch("message", { data });
  }

  public emitError(message: string) {
    this.dispatch("error", { message });
  }

  public emitClose(code: number, reason: string) {
    this.readyState = MockWebSocket.CLOSED;
    this.dispatch("close", { code, reason });
  }

  private dispatch(type: keyof typeof this.listeners, event: any) {
    for (const listener of this.listeners[type]) {
      listener(event);
    }
  }
}

const originalWebSocket = globalThis.WebSocket;

afterEach(() => {
  globalThis.WebSocket = originalWebSocket;
});

const waitFor = async (predicate: () => boolean) => {
  for (let i = 0; i < 20; i++) {
    if (predicate()) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 5));
  }
  throw new Error("condition was not met");
};

describe("FluxionDBClient initial connection retry", () => {
  it("retries initial failures and resolves when a later attempt succeeds", async () => {
    const sockets: MockWebSocket[] = [];

    class FlakyWebSocket extends MockWebSocket {
      constructor(url: string) {
        super(url);
        sockets.push(this);
        const attempt = sockets.length;

        if (attempt === 1) {
          setTimeout(() => {
            this.emitError("dial failed");
            this.emitClose(1006, "first attempt failed");
          }, 0);
        } else {
          setTimeout(() => {
            this.emitOpen();
            this.emitMessage(JSON.stringify({ type: "ready" }));
          }, 0);
        }
      }
    }

    // @ts-expect-error override for testing
    globalThis.WebSocket = FlakyWebSocket;

    const client = new FluxionDBClient({
      url: "ws://example",
      apiKey: "key",
      maxReconnectAttempts: 3,
      reconnectInterval: 5,
    });

    await client.connect();
    expect(sockets.length).toBe(2);
  });

  it("rejects after exhausting initial retry attempts", async () => {
    const sockets: MockWebSocket[] = [];

    class AlwaysFailWebSocket extends MockWebSocket {
      constructor(url: string) {
        super(url);
        sockets.push(this);
        setTimeout(() => {
          this.emitError("offline");
          this.emitClose(1006, "offline");
        }, 0);
      }
    }

    // @ts-expect-error override for testing
    globalThis.WebSocket = AlwaysFailWebSocket;

    const client = new FluxionDBClient({
      url: "ws://example",
      apiKey: "key",
      maxReconnectAttempts: 2,
      reconnectInterval: 5,
    });

    let caughtError: Error | null = null;
    try {
      await client.connect();
    } catch (error) {
      caughtError = error as Error;
    }

    expect(caughtError).not.toBeNull();
    expect(caughtError?.message).toContain("offline");
    expect(sockets.length).toBeGreaterThanOrEqual(3);
  });

  it("replays pending requests when the connection closes", async () => {
    const sockets: MockWebSocket[] = [];

    class HangingWebSocket extends MockWebSocket {
      constructor(url: string) {
        super(url);
        sockets.push(this);
        setTimeout(() => {
          this.emitOpen();
          this.emitMessage(JSON.stringify({ type: "ready" }));
        }, 0);
      }
    }

    // @ts-expect-error override for testing
    globalThis.WebSocket = HangingWebSocket;

    const client = new FluxionDBClient({
      url: "ws://example",
      apiKey: "key",
      maxReconnectAttempts: 2,
      reconnectInterval: 5,
    });

    await client.connect();

    const request = client.fetchCollections();
    await waitFor(() => sockets[0].sentFrames.length === 1);
    const sent = JSON.parse(sockets[0].sentFrames[0]);
    sockets[0].close(1006, "connection lost");
    await waitFor(
      () => sockets.length === 2 && sockets[1].sentFrames.length === 1,
    );

    const replayed = JSON.parse(sockets[1].sentFrames[0]);
    expect(replayed).toEqual(sent);

    sockets[1].emitMessage(
      JSON.stringify({ id: replayed.id, collections: ["after-reconnect"] }),
    );
    await expect(request).resolves.toEqual(["after-reconnect"]);
  });

  it("sends a request queued during connection only once", async () => {
    const sockets: MockWebSocket[] = [];

    class SlowReadyWebSocket extends MockWebSocket {
      constructor(url: string) {
        super(url);
        sockets.push(this);
        setTimeout(() => {
          this.emitOpen();
          this.emitMessage(JSON.stringify({ type: "ready" }));
        }, 10);
      }
    }

    // @ts-expect-error override for testing
    globalThis.WebSocket = SlowReadyWebSocket;

    const client = new FluxionDBClient({
      url: "ws://example",
      apiKey: "key",
      reconnectInterval: 5,
    });

    const request = client.fetchCollections();
    await waitFor(
      () => sockets.length === 1 && sockets[0].sentFrames.length === 1,
    );
    expect(sockets[0].sentFrames.length).toBe(1);

    const sent = JSON.parse(sockets[0].sentFrames[0]);
    sockets[0].emitMessage(
      JSON.stringify({ id: sent.id, collections: ["ok"] }),
    );
    await expect(request).resolves.toEqual(["ok"]);
  });

  it("rejects pending requests when the client is closed manually", async () => {
    const sockets: MockWebSocket[] = [];

    class HangingWebSocket extends MockWebSocket {
      constructor(url: string) {
        super(url);
        sockets.push(this);
        setTimeout(() => {
          this.emitOpen();
          this.emitMessage(JSON.stringify({ type: "ready" }));
        }, 0);
      }
    }

    // @ts-expect-error override for testing
    globalThis.WebSocket = HangingWebSocket;

    const client = new FluxionDBClient({
      url: "ws://example",
      apiKey: "key",
      reconnectInterval: 5,
    });

    await client.connect();

    const request = client.fetchCollections();
    await waitFor(() => sockets[0].sentFrames.length === 1);
    client.close();

    await expect(request).rejects.toThrow("WebSocket connection closed");
  });
});
