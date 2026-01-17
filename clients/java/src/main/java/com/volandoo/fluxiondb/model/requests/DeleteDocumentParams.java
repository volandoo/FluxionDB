package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

public final class DeleteDocumentParams {
    private final String col;
    private final String doc;

    public DeleteDocumentParams(String col, String doc) {
        this.col = Objects.requireNonNull(col, "col cannot be null");
        this.doc = Objects.requireNonNull(doc, "doc cannot be null");
    }

    public String getCol() {
        return col;
    }

    public String getDoc() {
        return doc;
    }

    @Override
    public String toString() {
        return "DeleteDocumentParams{col='" + col + "', doc='" + doc + "'}";
    }
}
