package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

public final class DeleteValueParams {
    private final String col;
    private final String key;

    public DeleteValueParams(String col, String key) {
        this.col = Objects.requireNonNull(col, "col cannot be null");
        this.key = Objects.requireNonNull(key, "key cannot be null");
    }

    public String getCol() {
        return col;
    }

    public String getKey() {
        return key;
    }

    @Override
    public String toString() {
        return "DeleteValueParams{col='" + col + "', key='" + key + "'}";
    }
}
