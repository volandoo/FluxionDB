package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

/**
 * Request to insert a single time series record.
 */
public final class InsertMessageRequest {
    private final long ts;
    private final String doc;
    private final String data;
    private final String col;

    public InsertMessageRequest(long ts, String doc, String data, String col) {
        this.ts = ts;
        this.doc = Objects.requireNonNull(doc, "doc cannot be null");
        this.data = Objects.requireNonNull(data, "data cannot be null");
        this.col = Objects.requireNonNull(col, "col cannot be null");
    }

    public long getTs() {
        return ts;
    }

    public String getDoc() {
        return doc;
    }

    public String getData() {
        return data;
    }

    public String getCol() {
        return col;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        InsertMessageRequest that = (InsertMessageRequest) o;
        return ts == that.ts && doc.equals(that.doc) && data.equals(that.data) && col.equals(that.col);
    }

    @Override
    public int hashCode() {
        return Objects.hash(ts, doc, data, col);
    }

    @Override
    public String toString() {
        return "InsertMessageRequest{" +
                "ts=" + ts +
                ", doc='" + doc + '\'' +
                ", data='" + data + '\'' +
                ", col='" + col + '\'' +
                '}';
    }
}
