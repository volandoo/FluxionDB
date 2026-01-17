package com.volandoo.fluxiondb.operations;

import com.volandoo.fluxiondb.connection.WebSocketManager;
import com.volandoo.fluxiondb.json.JsonBuilder;
import com.volandoo.fluxiondb.json.JsonParser;
import com.volandoo.fluxiondb.model.requests.*;
import com.volandoo.fluxiondb.protocol.MessageTypes;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;

/**
 * Handles key-value store operations.
 */
public class KeyValueOperations {

    private final WebSocketManager wsManager;

    public KeyValueOperations(WebSocketManager wsManager) {
        this.wsManager = wsManager;
    }

    public CompletableFuture<Void> setValue(SetValueParams params) {
        String data = new JsonBuilder()
                .add("col", params.getCol())
                .add("key", params.getKey())
                .add("value", params.getValue())
                .build();

        return wsManager.send(MessageTypes.SET_VALUE, data)
                .thenApply(response -> null);
    }

    public CompletableFuture<String> getValue(GetValueParams params) {
        String data = new JsonBuilder()
                .add("col", params.getCol())
                .add("key", params.getKey())
                .build();

        return wsManager.send(MessageTypes.GET_VALUE, data)
                .thenApply(response -> {
                    Map<String, Object> parsed = JsonParser.parseObject(response);
                    return JsonParser.getString(parsed, "value");
                });
    }

    public CompletableFuture<Map<String, String>> getValues(GetValuesParams params) {
        JsonBuilder builder = new JsonBuilder()
                .add("col", params.getCol());

        if (params.getKey() != null) {
            builder.add("key", params.getKey());
        }

        String data = builder.build();

        String messageType = params.getKey() != null ? MessageTypes.GET_VALUES : MessageTypes.GET_ALL_VALUES;

        return wsManager.send(messageType, data)
                .thenApply(response -> {
                    Map<String, Object> parsed = JsonParser.parseObject(response);
                    Map<String, Object> valuesMap = JsonParser.getObject(parsed, "values");

                    Map<String, String> result = new HashMap<>();
                    for (Map.Entry<String, Object> entry : valuesMap.entrySet()) {
                        result.put(entry.getKey(), entry.getValue() != null ? entry.getValue().toString() : null);
                    }

                    return result;
                });
    }

    public CompletableFuture<List<String>> getKeys(CollectionParam params) {
        String data = new JsonBuilder()
                .add("col", params.getCol())
                .build();

        return wsManager.send(MessageTypes.GET_ALL_KEYS, data)
                .thenApply(response -> {
                    Map<String, Object> parsed = JsonParser.parseObject(response);
                    List<Object> keysList = JsonParser.getArray(parsed, "keys");

                    List<String> result = new ArrayList<>();
                    for (Object item : keysList) {
                        result.add(item.toString());
                    }

                    return result;
                });
    }

    public CompletableFuture<Void> deleteValue(DeleteValueParams params) {
        String data = new JsonBuilder()
                .add("col", params.getCol())
                .add("key", params.getKey())
                .build();

        return wsManager.send(MessageTypes.REMOVE_VALUE, data)
                .thenApply(response -> null);
    }
}
