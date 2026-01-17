package com.volandoo.fluxiondb.model.requests;

import com.volandoo.fluxiondb.model.enums.ApiKeyScope;
import java.util.Objects;

public final class ManageApiKeyParams {
    private final String action;
    private final String key;
    private final String scope; // Only for "add" action

    private ManageApiKeyParams(String action, String key, String scope) {
        this.action = Objects.requireNonNull(action, "action cannot be null");
        this.key = Objects.requireNonNull(key, "key cannot be null");
        this.scope = scope;
    }

    public static ManageApiKeyParams add(String key, ApiKeyScope scope) {
        return new ManageApiKeyParams("add", key, scope.getValue());
    }

    public static ManageApiKeyParams remove(String key) {
        return new ManageApiKeyParams("remove", key, null);
    }

    public static ManageApiKeyParams list() {
        return new ManageApiKeyParams("list", "", null);
    }

    public String getAction() {
        return action;
    }

    public String getKey() {
        return key;
    }

    public String getScope() {
        return scope;
    }

    @Override
    public String toString() {
        return "ManageApiKeyParams{action='" + action + "', key='" + key + "', scope='" + scope + "'}";
    }
}
