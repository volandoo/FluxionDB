package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

public final class CollectionParam {
    private final String col;

    public CollectionParam(String col) {
        this.col = Objects.requireNonNull(col, "col cannot be null");
    }

    public String getCol() {
        return col;
    }

    @Override
    public String toString() {
        return "CollectionParam{col='" + col + "'}";
    }
}
