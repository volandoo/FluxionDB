package com.volandoo.fluxiondb.operations;

import com.volandoo.fluxiondb.connection.WebSocketManager;
import com.volandoo.fluxiondb.json.JsonBuilder;
import com.volandoo.fluxiondb.json.JsonParser;
import com.volandoo.fluxiondb.model.requests.DeleteCollectionParams;
import com.volandoo.fluxiondb.protocol.MessageTypes;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;

/**
 * Handles collection management operations.
 */
public class CollectionOperations {

    private final WebSocketManager wsManager;

    public CollectionOperations(WebSocketManager wsManager) {
        this.wsManager = wsManager;
    }

    public CompletableFuture<List<String>> fetchCollections() {
        return wsManager.send(MessageTypes.QUERY_COLLECTIONS, "{}")
                .thenApply(response -> {
                    Map<String, Object> parsed = JsonParser.parseObject(response);
                    List<Object> collectionsList = JsonParser.getArray(parsed, "collections");

                    List<String> result = new ArrayList<>();
                    for (Object item : collectionsList) {
                        result.add(item.toString());
                    }

                    return result;
                });
    }

    public CompletableFuture<Void> deleteCollection(DeleteCollectionParams params) {
        String data = new JsonBuilder()
                .add("col", params.getCol())
                .build();

        return wsManager.send(MessageTypes.DELETE_COLLECTION, data)
                .thenApply(response -> null);
    }
}
