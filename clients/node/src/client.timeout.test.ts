import { afterEach, describe, expect, it } from "bun:test";
import FluxionDBClient from "./client";
import TimeoutError from "./errors";

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

  public addEventListener(type: keyof typeof this.listeners, listener: any) {
    this.listeners[type].push(listener);
  }

  public removeEventListener(type: keyof typeof this.listeners, listener: any) {
    this.listeners[type] = this.listeners[type].filter((l) => l !== listener);
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  public send(_: string) {
    // overridden in subclasses
  }

  public close(code = 1000, reason = "") {
    this.readyState = MockWebSocket.CLOSED;
    this.dispatch("close", { code, reason });
  }

  protected emitOpen() {
    this.readyState = MockWebSocket.OPEN;
    this.dispatch("open", {});
  }

  protected emitMessage(data: any) {
    this.dispatch("message", { data });
  }

  protected emitError(message: string) {
    this.dispatch("error", { message });
  }

  protected emitClose(code: number, reason: string) {
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

describe("per-call timeouts", () => {
  it("fulfills the request before the timeout fires", async () => {
    class FastWebSocket extends MockWebSocket {
      constructor(url: string) {
        super(url);
        setTimeout(() => {
          this.emitOpen();
          this.emitMessage(JSON.stringify({ type: "ready" }));
        }, 0);
      }

      public override send(payload: string) {
        const { id } = JSON.parse(payload);
        setTimeout(() => {
          this.emitMessage(JSON.stringify({ id, value: "ok" }));
        }, 5);
      }
    }

    // @ts-expect-error override for testing
    globalThis.WebSocket = FastWebSocket;

    const client = new FluxionDBClient({ url: "ws://example", apiKey: "key" });
    const value = await client.getValue({ col: "col", key: "k" }, 50);
    expect(value).toBe("ok");
  });

  it("rejects with TimeoutError and cleans inflight requests", async () => {
    class SlowWebSocket extends MockWebSocket {
      constructor(url: string) {
        super(url);
        setTimeout(() => {
          this.emitOpen();
          this.emitMessage(JSON.stringify({ type: "ready" }));
        }, 0);
      }

      public override send(_: string) {
        // Never respond to trigger the timeout
      }
    }

    // @ts-expect-error override for testing
    globalThis.WebSocket = SlowWebSocket;

    const client = new FluxionDBClient({ url: "ws://example", apiKey: "key" });

    let caught: unknown;
    try {
      await client.getValue({ col: "col", key: "k" }, 10);
    } catch (error) {
      caught = error;
    }

    expect(caught).toBeInstanceOf(TimeoutError);
    const inflight = (client as any).inflightRequests as Record<string, unknown>;
    expect(Object.keys(inflight).length).toBe(0);
  });
});
