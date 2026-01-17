package com.volandoo.fluxiondb.exceptions;

/**
 * Exception thrown when authentication with FluxionDB server fails.
 */
public class AuthenticationException extends FluxionDBException {

    public AuthenticationException(String message) {
        super(message);
    }

    public AuthenticationException(String message, Throwable cause) {
        super(message, cause);
    }
}
