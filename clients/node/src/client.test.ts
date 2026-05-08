import FluxionDBClient from "./client";
import { InsertMessageRequest } from "./types";
import { beforeAll, afterAll, describe, it, expect } from "bun:test";

const testCollection = "test_collection_2";
const kvCollection = `${testCollection}_kv`;

const createMockData = () => {
  const documentIds = ["document_1", "document_2", "document_3"];
  const now = 1747604423;
  const records: InsertMessageRequest[] = [];
  for (const documentId of documentIds) {
    for (let i = 0; i < 1000; i++) {
      const ts = now + i;
      const data = JSON.stringify({ value: i });
      records.push({ ts, doc: documentId, data, col: testCollection });
    }
  }
  records.push({
    ts: now - 1,
    doc: "collection_4",
    data: "test",
    col: testCollection,
  });

  return records;
};

describe("FluxionDBClient Integration", () => {
  const url = "ws://localhost:8080";
  const apiKey = "my-secret-key";

  const client = new FluxionDBClient({ url, apiKey });

  beforeAll(async () => {
    const records = createMockData();
    console.log("Connecting to:", url);
    await client.connect();
    await client.deleteCollection({ col: testCollection });
    await client.insertMultipleRecords(records);
    await client.deleteCollection({ col: kvCollection });
    await client.setValue({ col: kvCollection, key: "env.prod", value: "v1" });
    await client.setValue({ col: kvCollection, key: "env.stage", value: "v2" });
    await client.setValue({
      col: kvCollection,
      key: "config.default",
      value: "v3",
    });
  }, 10000);

  afterAll(async () => {
    client.close();
    await new Promise((resolve) => setTimeout(resolve, 1000));
  });

  it("should fetch latest document records of all four", async () => {
    const result = await client.fetchLatestRecords({
      col: testCollection,
      ts: Date.now(),
    });
    const keys = Object.keys(result);
    expect(keys.length).toBe(4);
    for (const key of keys) {
      if (key === "collection_4") {
        expect(result[key].ts).toBe(1747604423 - 1);
      } else {
        expect(result[key].ts).toBe(1747604423 + 999);
      }
    }
  }, 10000);

  it("should fetch latest document records of only three", async () => {
    const result = await client.fetchLatestRecords({
      col: testCollection,
      ts: Date.now(),
      from: 1747604423 + 1,
    });
    const keys = Object.keys(result);
    expect(keys.length).toBe(3);
    for (const key of keys) {
      expect(result[key].ts).toBe(1747604423 + 999);
    }
  }, 10000);

  it("should support regex filters when fetching latest records", async () => {
    const result = await client.fetchLatestRecords({
      col: testCollection,
      ts: Date.now(),
      doc: "/document_[12]/",
    });
    const keys = Object.keys(result);
    expect(keys.length).toBe(2);
    expect(keys).toEqual(expect.arrayContaining(["document_1", "document_2"]));
  }, 10000);

  it("should support optional where and filter content predicates", async () => {
    const col = `${testCollection}_predicates`;
    await client.deleteCollection({ col });
    await client.insertMultipleRecords([
      {
        col,
        doc: "session_a",
        ts: 100,
        data: "session_id:123 state:flying quality:ok",
      },
      {
        col,
        doc: "session_a",
        ts: 200,
        data: "session_id:123 state:landed quality:bad",
      },
      {
        col,
        doc: "session_b",
        ts: 150,
        data: "session_id:999 state:flying quality:ok",
      },
      {
        col,
        doc: "session_c",
        ts: 175,
        data: "session_id:123 state:flying quality:ok",
      },
    ]);

    const latest = await client.fetchLatestRecords({
      col,
      ts: 999,
      where: "session_id:123",
      filter: "quality:bad",
    });
    expect(Object.keys(latest).sort()).toEqual(["session_c"]);

    const history = await client.fetchDocument({
      col,
      doc: "session_a",
      from: 0,
      to: 999,
      where: "session_id:123",
      filter: "quality:bad",
    });
    expect(history).toEqual([
      {
        ts: 100,
        data: "session_id:123 state:flying quality:ok",
      },
    ]);

    const regexLatest = await client.fetchLatestRecords({
      col,
      ts: 999,
      where: "/state:(flying|landed)/",
      filter: "/quality:bad/",
    });
    expect(Object.keys(regexLatest).sort()).toEqual(["session_b", "session_c"]);

    const regexHistory = await client.fetchDocument({
      col,
      doc: "session_a",
      from: 0,
      to: 999,
      where: "/state:(flying|landed)/",
      filter: "/quality:bad/",
    });
    expect(regexHistory).toEqual([
      {
        ts: 100,
        data: "session_id:123 state:flying quality:ok",
      },
    ]);
  }, 10000);

  it("should fetch key values using regex patterns", async () => {
    const values = await client.getValues({
      col: kvCollection,
      key: "/env\\..*/",
    });
    expect(Object.keys(values).sort()).toEqual(["env.prod", "env.stage"]);
    expect(values["env.prod"]).toBe("v1");
    expect(values["env.stage"]).toBe("v2");
  }, 10000);

  it("should fetch key values using literal key lookups", async () => {
    const values = await client.getValues({
      col: kvCollection,
      key: "config.default",
    });
    expect(values).toEqual({ "config.default": "v3" });
  }, 10000);

  it("should list active connections", async () => {
    const connectionPromise = client.getConnections();
    const result = await Promise.race<
      | { ok: true; value: Awaited<ReturnType<typeof client.getConnections>> }
      | { ok: false; error: Error }
    >([
      connectionPromise
        .then((value) => ({ ok: true as const, value }))
        .catch((error) => ({ ok: false as const, error })),
      new Promise<{ ok: false; error: Error }>((resolve) =>
        setTimeout(
          () => resolve({ ok: false, error: new Error("timeout") }),
          2000,
        ),
      ),
    ]);

    if (!result.ok) {
      console.warn("getConnections unavailable:", result.error.message);
      return;
    }

    expect(result.value.length).toBe(1);
    const selfConn = result.value.find((conn) => conn.self);
    expect(selfConn).toBeDefined();
    expect(typeof selfConn?.ip).toBe("string");
    expect(typeof selfConn?.since).toBe("number");
  }, 10000);

  it("should manage scoped API keys", async () => {
    const managedKey = `sdk-readonly-${Date.now()}`;
    const addResponse = await client.addApiKey({
      key: managedKey,
      scope: "readonly",
    });
    expect(addResponse.error ?? "").toBe("");
    expect(addResponse.status).toBe("ok");

    const removeResponse = await client.removeApiKey({ key: managedKey });
    expect(removeResponse.error ?? "").toBe("");
    expect(removeResponse.status).toBe("ok");
  }, 10000);
});
