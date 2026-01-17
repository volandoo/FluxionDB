package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

public final class DeleteCollectionParams {
    private final String col;

    public DeleteCollectionParams(String col) {
        this.col = Objects.requireNonNull(col, "col cannot be null");
    }

    public String getCol() {
        return col;
    }

    @Override
    public String toString() {
        return "DeleteCollectionParams{col='" + col + "'}";
    }
}
