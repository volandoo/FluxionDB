package com.volandoo.fluxiondb.exceptions;

/**
 * Base exception for all FluxionDB client errors.
 */
public class FluxionDBException extends RuntimeException {

    public FluxionDBException(String message) {
        super(message);
    }

    public FluxionDBException(String message, Throwable cause) {
        super(message, cause);
    }

    public FluxionDBException(Throwable cause) {
        super(cause);
    }
}
