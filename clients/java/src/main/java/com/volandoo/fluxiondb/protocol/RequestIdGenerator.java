package com.volandoo.fluxiondb.protocol;

import java.security.SecureRandom;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Generates unique request IDs for WebSocket messages.
 */
public final class RequestIdGenerator {

    private static final AtomicLong counter = new AtomicLong(0);
    private static final SecureRandom random = new SecureRandom();
    private static final String ALPHABET = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    private RequestIdGenerator() {
        // Utility class, prevent instantiation
    }

    /**
     * Generates a unique request ID combining timestamp, counter, and random characters.
     *
     * @return a unique request ID string
     */
    public static String generate() {
        long timestamp = System.currentTimeMillis();
        long count = counter.incrementAndGet();
        String randomPart = generateRandomString(8);

        return timestamp + "-" + count + "-" + randomPart;
    }

    private static String generateRandomString(int length) {
        StringBuilder sb = new StringBuilder(length);
        for (int i = 0; i < length; i++) {
            sb.append(ALPHABET.charAt(random.nextInt(ALPHABET.length())));
        }
        return sb.toString();
    }
}
