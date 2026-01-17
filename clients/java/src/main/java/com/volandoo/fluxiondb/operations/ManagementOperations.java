package com.volandoo.fluxiondb.operations;

import com.volandoo.fluxiondb.connection.WebSocketManager;
import com.volandoo.fluxiondb.json.JsonBuilder;
import com.volandoo.fluxiondb.json.JsonParser;
import com.volandoo.fluxiondb.model.enums.ApiKeyScope;
import com.volandoo.fluxiondb.model.requests.ManageApiKeyParams;
import com.volandoo.fluxiondb.model.responses.ApiKeyInfo;
import com.volandoo.fluxiondb.model.responses.ConnectionInfo;
import com.volandoo.fluxiondb.protocol.MessageTypes;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;

/**
 * Handles API key and connection management operations.
 */
public class ManagementOperations {

    private final WebSocketManager wsManager;

    public ManagementOperations(WebSocketManager wsManager) {
        this.wsManager = wsManager;
    }

    public CompletableFuture<List<ConnectionInfo>> getConnections() {
        return wsManager.send(MessageTypes.CONNECTIONS, "{}")
                .thenApply(response -> {
                    Map<String, Object> parsed = JsonParser.parseObject(response);
                    List<Object> connectionsList = JsonParser.getArray(parsed, "connections");

                    List<ConnectionInfo> result = new ArrayList<>();
                    for (Object item : connectionsList) {
                        @SuppressWarnings("unchecked")
                        Map<String, Object> connObj = (Map<String, Object>) item;
                        String ip = JsonParser.getString(connObj, "ip");
                        long since = JsonParser.getLong(connObj, "since");
                        boolean self = JsonParser.getBoolean(connObj, "self");
                        String name = JsonParser.getString(connObj, "name");

                        result.add(new ConnectionInfo(ip, since, self, name));
                    }

                    return result;
                });
    }

    public CompletableFuture<String> addApiKey(String key, ApiKeyScope scope) {
        ManageApiKeyParams params = ManageApiKeyParams.add(key, scope);
        String data = new JsonBuilder()
                .add("action", params.getAction())
                .add("key", params.getKey())
                .add("scope", params.getScope())
                .build();

        return wsManager.send(MessageTypes.MANAGE_API_KEYS, data)
                .thenApply(response -> {
                    Map<String, Object> parsed = JsonParser.parseObject(response);
                    String status = JsonParser.getString(parsed, "status");
                    if (status != null && "ok".equals(status)) {
                        return "API key added successfully";
                    }
                    String error = JsonParser.getString(parsed, "error");
                    if (error != null) {
                        throw new RuntimeException("Failed to add API key: " + error);
                    }
                    return status;
                });
    }

    public CompletableFuture<String> removeApiKey(String key) {
        ManageApiKeyParams params = ManageApiKeyParams.remove(key);
        String data = new JsonBuilder()
                .add("action", params.getAction())
                .add("key", params.getKey())
                .build();

        return wsManager.send(MessageTypes.MANAGE_API_KEYS, data)
                .thenApply(response -> {
                    Map<String, Object> parsed = JsonParser.parseObject(response);
                    String status = JsonParser.getString(parsed, "status");
                    if (status != null && "ok".equals(status)) {
                        return "API key removed successfully";
                    }
                    String error = JsonParser.getString(parsed, "error");
                    if (error != null) {
                        throw new RuntimeException("Failed to remove API key: " + error);
                    }
                    return status;
                });
    }

    public CompletableFuture<List<ApiKeyInfo>> listApiKeys() {
        ManageApiKeyParams params = ManageApiKeyParams.list();
        String data = new JsonBuilder()
                .add("action", params.getAction())
                .add("key", "")
                .build();

        return wsManager.send(MessageTypes.MANAGE_API_KEYS, data)
                .thenApply(response -> {
                    Map<String, Object> parsed = JsonParser.parseObject(response);
                    List<Object> keysList = JsonParser.getArray(parsed, "keys");

                    List<ApiKeyInfo> result = new ArrayList<>();
                    for (Object item : keysList) {
                        @SuppressWarnings("unchecked")
                        Map<String, Object> keyObj = (Map<String, Object>) item;
                        String key = JsonParser.getString(keyObj, "key");
                        String scope = JsonParser.getString(keyObj, "scope");
                        boolean deletable = JsonParser.getBoolean(keyObj, "deletable");

                        result.add(new ApiKeyInfo(key, scope, deletable));
                    }

                    return result;
                });
    }
}
