package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

public final class DeleteRecord {
    private final String col;
    private final String doc;
    private final long ts;

    public DeleteRecord(String col, String doc, long ts) {
        this.col = Objects.requireNonNull(col, "col cannot be null");
        this.doc = Objects.requireNonNull(doc, "doc cannot be null");
        this.ts = ts;
    }

    public String getCol() {
        return col;
    }

    public String getDoc() {
        return doc;
    }

    public long getTs() {
        return ts;
    }

    @Override
    public String toString() {
        return "DeleteRecord{col='" + col + "', doc='" + doc + "', ts=" + ts + "}";
    }
}
