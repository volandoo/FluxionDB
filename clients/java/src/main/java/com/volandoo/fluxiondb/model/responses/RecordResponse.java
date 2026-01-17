package com.volandoo.fluxiondb.model.responses;

import java.util.Objects;

public final class RecordResponse {
    private final long ts;
    private final String data;

    public RecordResponse(long ts, String data) {
        this.ts = ts;
        this.data = data;
    }

    public long getTs() {
        return ts;
    }

    public String getData() {
        return data;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        RecordResponse that = (RecordResponse) o;
        return ts == that.ts && Objects.equals(data, that.data);
    }

    @Override
    public int hashCode() {
        return Objects.hash(ts, data);
    }

    @Override
    public String toString() {
        return "RecordResponse{ts=" + ts + ", data='" + data + "'}";
    }
}
