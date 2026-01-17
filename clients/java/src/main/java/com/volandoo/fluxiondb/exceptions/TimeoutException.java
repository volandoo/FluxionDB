package com.volandoo.fluxiondb.exceptions;

/**
 * Exception thrown when a request times out waiting for a response.
 */
public class TimeoutException extends FluxionDBException {

    public TimeoutException(String message) {
        super(message);
    }

    public TimeoutException(String message, Throwable cause) {
        super(message, cause);
    }
}
