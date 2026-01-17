package com.volandoo.fluxiondb.connection;

/**
 * Implements exponential backoff strategy for WebSocket reconnection.
 */
public class ReconnectionStrategy {

    private final int maxAttempts;
    private final long baseIntervalMs;

    public ReconnectionStrategy(int maxAttempts, long baseIntervalMs) {
        if (maxAttempts < 0) {
            throw new IllegalArgumentException("maxAttempts must be non-negative");
        }
        if (baseIntervalMs < 0) {
            throw new IllegalArgumentException("baseIntervalMs must be non-negative");
        }

        this.maxAttempts = maxAttempts;
        this.baseIntervalMs = baseIntervalMs;
    }

    /**
     * Calculates the delay in milliseconds for a given attempt number.
     * Implements exponential backoff: baseInterval * attemptNumber
     * Capped at 60 seconds to avoid excessive delays.
     *
     * @param attemptNumber the current reconnection attempt (1-indexed)
     * @return delay in milliseconds
     */
    public long getDelayMs(int attemptNumber) {
        if (attemptNumber < 1) {
            throw new IllegalArgumentException("attemptNumber must be >= 1");
        }

        long delay = baseIntervalMs * attemptNumber;
        return Math.min(delay, 60000); // Cap at 60 seconds
    }

    /**
     * Determines if a retry should be attempted.
     *
     * @param attemptNumber the current reconnection attempt (1-indexed)
     * @return true if should retry, false otherwise
     */
    public boolean shouldRetry(int attemptNumber) {
        return attemptNumber < maxAttempts;
    }

    public int getMaxAttempts() {
        return maxAttempts;
    }

    public long getBaseIntervalMs() {
        return baseIntervalMs;
    }

    @Override
    public String toString() {
        return "ReconnectionStrategy{maxAttempts=" + maxAttempts + ", baseIntervalMs=" + baseIntervalMs + "}";
    }
}
