package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

/**
 * Parameters for fetching latest records per document.
 */
public final class FetchLatestRecordsParams {
    private final String col;
    private final long ts;
    private final String doc;
    private final Long from; // Optional

    private FetchLatestRecordsParams(Builder builder) {
        this.col = Objects.requireNonNull(builder.col, "col cannot be null");
        this.ts = builder.ts;
        this.doc = builder.doc;
        this.from = builder.from;
    }

    public String getCol() {
        return col;
    }

    public long getTs() {
        return ts;
    }

    public String getDoc() {
        return doc;
    }

    public Long getFrom() {
        return from;
    }

    public static Builder builder() {
        return new Builder();
    }

    public static class Builder {
        private String col;
        private long ts;
        private String doc;
        private Long from;

        public Builder col(String col) {
            this.col = col;
            return this;
        }

        public Builder ts(long ts) {
            this.ts = ts;
            return this;
        }

        public Builder doc(String doc) {
            this.doc = doc;
            return this;
        }

        public Builder from(long from) {
            this.from = from;
            return this;
        }

        public FetchLatestRecordsParams build() {
            return new FetchLatestRecordsParams(this);
        }
    }

    @Override
    public String toString() {
        return "FetchLatestRecordsParams{" +
                "col='" + col + '\'' +
                ", ts=" + ts +
                ", doc='" + doc + '\'' +
                ", from=" + from +
                '}';
    }
}
