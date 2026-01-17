package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

public final class GetValuesParams {
    private final String col;
    private final String key; // Optional - can be regex pattern

    public GetValuesParams(String col, String key) {
        this.col = Objects.requireNonNull(col, "col cannot be null");
        this.key = key; // Nullable
    }

    public GetValuesParams(String col) {
        this(col, null);
    }

    public String getCol() {
        return col;
    }

    public String getKey() {
        return key;
    }

    @Override
    public String toString() {
        return "GetValuesParams{col='" + col + "', key='" + key + "'}";
    }
}
