package com.volandoo.fluxiondb.operations;

import com.volandoo.fluxiondb.connection.WebSocketManager;
import com.volandoo.fluxiondb.json.JsonBuilder;
import com.volandoo.fluxiondb.json.JsonParser;
import com.volandoo.fluxiondb.model.requests.*;
import com.volandoo.fluxiondb.model.responses.RecordResponse;
import com.volandoo.fluxiondb.protocol.MessageTypes;

import java.util.*;
import java.util.concurrent.CompletableFuture;

/**
 * Handles time series data operations.
 */
public class TimeSeriesOperations {

    private final WebSocketManager wsManager;

    public TimeSeriesOperations(WebSocketManager wsManager) {
        this.wsManager = wsManager;
    }

    public CompletableFuture<Void> insertSingleRecord(InsertMessageRequest request) {
        return insertMultipleRecords(Collections.singletonList(request));
    }

    public CompletableFuture<Void> insertMultipleRecords(List<InsertMessageRequest> requests) {
        StringBuilder arrayJson = new StringBuilder("[");
        for (int i = 0; i < requests.size(); i++) {
            if (i > 0) arrayJson.append(",");
            InsertMessageRequest req = requests.get(i);

            arrayJson.append(new JsonBuilder()
                    .add("ts", req.getTs())
                    .add("doc", req.getDoc())
                    .add("data", req.getData())
                    .add("col", req.getCol())
                    .build());
        }
        arrayJson.append("]");

        return wsManager.send(MessageTypes.INSERT, arrayJson.toString())
                .thenApply(response -> null);
    }

    public CompletableFuture<Map<String, RecordResponse>> fetchLatestRecords(FetchLatestRecordsParams params) {
        JsonBuilder builder = new JsonBuilder()
                .add("col", params.getCol())
                .add("ts", params.getTs());

        if (params.getDoc() != null) {
            builder.add("doc", params.getDoc());
        }
        if (params.getFrom() != null) {
            builder.add("from", params.getFrom());
        }

        String data = builder.build();

        return wsManager.send(MessageTypes.QUERY_RECORDS, data)
                .thenApply(response -> {
                    Map<String, Object> parsed = JsonParser.parseObject(response);
                    Map<String, Object> recordsMap = JsonParser.getObject(parsed, "records");

                    Map<String, RecordResponse> result = new HashMap<>();
                    for (Map.Entry<String, Object> entry : recordsMap.entrySet()) {
                        @SuppressWarnings("unchecked")
                        Map<String, Object> recordObj = (Map<String, Object>) entry.getValue();
                        long ts = JsonParser.getLong(recordObj, "ts");
                        String recordData = JsonParser.getString(recordObj, "data");
                        result.put(entry.getKey(), new RecordResponse(ts, recordData));
                    }

                    return result;
                });
    }

    public CompletableFuture<List<RecordResponse>> fetchDocument(FetchRecordsParams params) {
        JsonBuilder builder = new JsonBuilder()
                .add("col", params.getCol())
                .add("doc", params.getDoc())
                .add("from", params.getFrom())
                .add("to", params.getTo());

        if (params.getLimit() != null) {
            builder.add("limit", params.getLimit());
        }
        if (params.getReverse() != null) {
            builder.add("reverse", params.getReverse());
        }

        String data = builder.build();

        return wsManager.send(MessageTypes.QUERY_DOCUMENT, data)
                .thenApply(response -> {
                    Map<String, Object> parsed = JsonParser.parseObject(response);
                    List<Object> recordsList = JsonParser.getArray(parsed, "records");

                    List<RecordResponse> result = new ArrayList<>();
                    for (Object item : recordsList) {
                        @SuppressWarnings("unchecked")
                        Map<String, Object> recordObj = (Map<String, Object>) item;
                        long ts = JsonParser.getLong(recordObj, "ts");
                        String recordData = JsonParser.getString(recordObj, "data");
                        result.add(new RecordResponse(ts, recordData));
                    }

                    return result;
                });
    }

    public CompletableFuture<Void> deleteDocument(DeleteDocumentParams params) {
        String data = new JsonBuilder()
                .add("col", params.getCol())
                .add("doc", params.getDoc())
                .build();

        return wsManager.send(MessageTypes.DELETE_DOCUMENT, data)
                .thenApply(response -> null);
    }

    public CompletableFuture<Void> deleteRecord(DeleteRecord params) {
        String data = new JsonBuilder()
                .add("col", params.getCol())
                .add("doc", params.getDoc())
                .add("ts", params.getTs())
                .build();

        return wsManager.send(MessageTypes.DELETE_RECORD, data)
                .thenApply(response -> null);
    }

    public CompletableFuture<Void> deleteMultipleRecords(List<DeleteRecord> records) {
        StringBuilder arrayJson = new StringBuilder("[");
        for (int i = 0; i < records.size(); i++) {
            if (i > 0) arrayJson.append(",");
            DeleteRecord rec = records.get(i);

            arrayJson.append(new JsonBuilder()
                    .add("col", rec.getCol())
                    .add("doc", rec.getDoc())
                    .add("ts", rec.getTs())
                    .build());
        }
        arrayJson.append("]");

        return wsManager.send(MessageTypes.DELETE_MULTIPLE_RECORDS, arrayJson.toString())
                .thenApply(response -> null);
    }

    public CompletableFuture<Void> deleteRecordsRange(DeleteRecordsRange params) {
        String data = new JsonBuilder()
                .add("col", params.getCol())
                .add("doc", params.getDoc())
                .add("fromTs", params.getFromTs())
                .add("toTs", params.getToTs())
                .build();

        return wsManager.send(MessageTypes.DELETE_RECORDS_RANGE, data)
                .thenApply(response -> null);
    }
}
