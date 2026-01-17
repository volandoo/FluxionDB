package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

public final class SetValueParams {
    private final String col;
    private final String key;
    private final String value;

    public SetValueParams(String col, String key, String value) {
        this.col = Objects.requireNonNull(col, "col cannot be null");
        this.key = Objects.requireNonNull(key, "key cannot be null");
        this.value = Objects.requireNonNull(value, "value cannot be null");
    }

    public String getCol() {
        return col;
    }

    public String getKey() {
        return key;
    }

    public String getValue() {
        return value;
    }

    @Override
    public String toString() {
        return "SetValueParams{col='" + col + "', key='" + key + "', value='" + value + "'}";
    }
}
