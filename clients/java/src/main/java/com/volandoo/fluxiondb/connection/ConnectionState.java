package com.volandoo.fluxiondb.connection;

/**
 * WebSocket connection state.
 */
public enum ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    CLOSED
}
