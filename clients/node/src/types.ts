export type InsertMessageRequest = {
  ts: number;
  doc: string;
  data: string;
  col: string;
};

export type ApiKeyScope = "readonly" | "read_write" | "read_write_delete";

export type ManageApiKeyParams = {
  action: "add" | "remove";
  key: string;
  scope?: ApiKeyScope;
};

export type ApiKeyEntry = {
  key: string;
  scope: ApiKeyScope;
  deletable: boolean;
};

export type AddApiKeyParams = {
  key: string;
  scope: ApiKeyScope;
};

export type RemoveApiKeyParams = {
  key: string;
};

export type ManageApiKeyResponse = {
  id: string;
  status?: string;
  error?: string;
  scope?: string;
  keys?: ApiKeyEntry[];
};

export type InsertMessageResponse = {
  id: string;
};

export type QueryResponse = {
  id: string;
  records: {
    [key: string]: {
      ts: number;
      data: string;
    };
  };
};

export type QueryCollectionResponse = {
  id: string;
  records: {
    ts: number;
    data: string;
  }[];
};

export type CollectionsResponse = {
  id: string;
  collections: string[];
};

// key value response

export type KeyValueResponse = {
  id: string;
  value: string;
};

export type KeyValueAllKeysResponse = {
  id: string;
  keys: string[];
};

export type KeyValueAllValuesResponse = {
  id: string;
  values: { [key: string]: string };
};

export type ConnectionInfo = {
  ip: string;
  since: number;
  self: boolean;
  name?: string | null;
};

export type ConnectionsResponse = {
  id: string;
  connections: ConnectionInfo[];
};

export type CollectionParam = {
  col: string;
};

export type FetchLatestRecordsParams = {
  col: string;
  ts: number;
  doc?: string;
  from?: number;
};

export type DeleteCollectionParams = {
  col: string;
};

export type FetchRecordsParams = {
  col: string;
  doc: string;
  from: number;
  to: number;
  limit?: number;
  reverse?: boolean;
};

export type DeleteDocumentParams = {
  doc: string;
  col: string;
};

export type DeleteRecord = {
  doc: string;
  col: string;
  ts: number;
};

export type DeleteRecordsRange = {
  doc: string;
  col: string;
  fromTs: number;
  toTs: number;
};

export type SetValueParams = {
  col: string;
  key: string;
  value: string;
};

export type GetValueParams = {
  col: string;
  key: string;
};

export type GetValuesParams = {
  col: string;
  key?: string;
};

export type DeleteValueParams = {
  col: string;
  key: string;
};
