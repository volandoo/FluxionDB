package com.volandoo.fluxiondb.model.responses;

import java.util.Objects;

public final class ApiKeyInfo {
    private final String key;
    private final String scope;
    private final boolean deletable;

    public ApiKeyInfo(String key, String scope, boolean deletable) {
        this.key = key;
        this.scope = scope;
        this.deletable = deletable;
    }

    public String getKey() {
        return key;
    }

    public String getScope() {
        return scope;
    }

    public boolean isDeletable() {
        return deletable;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        ApiKeyInfo that = (ApiKeyInfo) o;
        return deletable == that.deletable &&
                Objects.equals(key, that.key) &&
                Objects.equals(scope, that.scope);
    }

    @Override
    public int hashCode() {
        return Objects.hash(key, scope, deletable);
    }

    @Override
    public String toString() {
        return "ApiKeyInfo{key='" + key + "', scope='" + scope + "', deletable=" + deletable + "}";
    }
}
