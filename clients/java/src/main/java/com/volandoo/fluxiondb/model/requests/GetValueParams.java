package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

public final class GetValueParams {
    private final String col;
    private final String key;

    public GetValueParams(String col, String key) {
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
        return "GetValueParams{col='" + col + "', key='" + key + "'}";
    }
}
