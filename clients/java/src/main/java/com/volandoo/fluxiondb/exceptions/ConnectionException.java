package com.volandoo.fluxiondb.exceptions;

/**
 * Exception thrown when WebSocket connection fails or is lost.
 */
public class ConnectionException extends FluxionDBException {

    public ConnectionException(String message) {
        super(message);
    }

    public ConnectionException(String message, Throwable cause) {
        super(message, cause);
    }

    public ConnectionException(Throwable cause) {
        super(cause);
    }
}
