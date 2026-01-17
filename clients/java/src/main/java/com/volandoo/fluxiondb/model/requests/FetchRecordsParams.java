package com.volandoo.fluxiondb.model.requests;

import java.util.Objects;

/**
 * Parameters for fetching document records within a time range.
 */
public final class FetchRecordsParams {
    private final String col;
    private final String doc;
    private final long from;
    private final long to;
    private final Integer limit;   // Optional
    private final Boolean reverse; // Optional

    private FetchRecordsParams(Builder builder) {
        this.col = Objects.requireNonNull(builder.col, "col cannot be null");
        this.doc = Objects.requireNonNull(builder.doc, "doc cannot be null");
        this.from = builder.from;
        this.to = builder.to;
        this.limit = builder.limit;
        this.reverse = builder.reverse;
    }

    public String getCol() {
        return col;
    }

    public String getDoc() {
        return doc;
    }

    public long getFrom() {
        return from;
    }

    public long getTo() {
        return to;
    }

    public Integer getLimit() {
        return limit;
    }

    public Boolean getReverse() {
        return reverse;
    }

    public static Builder builder() {
        return new Builder();
    }

    public static class Builder {
        private String col;
        private String doc;
        private long from;
        private long to;
        private Integer limit;
        private Boolean reverse;

        public Builder col(String col) {
            this.col = col;
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

        public Builder to(long to) {
            this.to = to;
            return this;
        }

        public Builder limit(int limit) {
            this.limit = limit;
            return this;
        }

        public Builder reverse(boolean reverse) {
            this.reverse = reverse;
            return this;
        }

        public FetchRecordsParams build() {
            return new FetchRecordsParams(this);
        }
    }

    @Override
    public String toString() {
        return "FetchRecordsParams{" +
                "col='" + col + '\'' +
                ", doc='" + doc + '\'' +
                ", from=" + from +
                ", to=" + to +
                ", limit=" + limit +
                ", reverse=" + reverse +
                '}';
    }
}
