package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

public final class DeleteRecordsRange {
    private final String col;
    private final String doc;
    private final long fromTs;
    private final long toTs;

    public DeleteRecordsRange(String col, String doc, long fromTs, long toTs) {
        this.col = Objects.requireNonNull(col, "col cannot be null");
        this.doc = Objects.requireNonNull(doc, "doc cannot be null");
        this.fromTs = fromTs;
        this.toTs = toTs;
    }

    public String getCol() {
        return col;
    }

    public String getDoc() {
        return doc;
    }

    public long getFromTs() {
        return fromTs;
    }

    public long getToTs() {
        return toTs;
    }

    @Override
    public String toString() {
        return "DeleteRecordsRange{col='" + col + "', doc='" + doc + "', fromTs=" + fromTs + ", toTs=" + toTs + "}";
    }
}
