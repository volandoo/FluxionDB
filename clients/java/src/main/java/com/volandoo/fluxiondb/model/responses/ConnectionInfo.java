package com.volandoo.fluxiondb.model.responses;

import java.util.Objects;

public final class ConnectionInfo {
    private final String ip;
    private final long since;
    private final boolean self;
    private final String name; // Optional

    public ConnectionInfo(String ip, long since, boolean self, String name) {
        this.ip = ip;
        this.since = since;
        this.self = self;
        this.name = name;
    }

    public String getIp() {
        return ip;
    }

    public long getSince() {
        return since;
    }

    public boolean isSelf() {
        return self;
    }

    public String getName() {
        return name;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        ConnectionInfo that = (ConnectionInfo) o;
        return since == that.since && self == that.self &&
                Objects.equals(ip, that.ip) && Objects.equals(name, that.name);
    }

    @Override
    public int hashCode() {
        return Objects.hash(ip, since, self, name);
    }

    @Override
    public String toString() {
        return "ConnectionInfo{ip='" + ip + "', since=" + since + ", self=" + self + ", name='" + name + "'}";
    }
}
