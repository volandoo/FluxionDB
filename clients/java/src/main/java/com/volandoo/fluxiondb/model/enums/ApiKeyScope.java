package com.volandoo.fluxiondb.model.enums;

/**
 * API key permission scope levels.
 */
public enum ApiKeyScope {
    READONLY("readonly"),
    READ_WRITE("read_write"),
    READ_WRITE_DELETE("read_write_delete");

    private final String value;

    ApiKeyScope(String value) {
        this.value = value;
    }

    public String getValue() {
        return value;
    }

    public static ApiKeyScope fromValue(String value) {
        for (ApiKeyScope scope : values()) {
            if (scope.value.equals(value)) {
                return scope;
            }
        }
        throw new IllegalArgumentException("Unknown ApiKeyScope: " + value);
    }

    @Override
    public String toString() {
        return value;
    }
}
