package com.volandoo.fluxiondb.json;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Minimal JSON parser for parsing JSON responses without third-party dependencies.
 * Handles objects, arrays, strings, numbers, booleans, and null values.
 */
public class JsonParser {

    private String json;
    private int pos;

    public JsonParser(String json) {
        this.json = json;
        this.pos = 0;
    }

    public static Map<String, Object> parseObject(String json) {
        return new JsonParser(json).parseObjectInternal();
    }

    public static List<Object> parseArray(String json) {
        return new JsonParser(json).parseArrayInternal();
    }

    private Map<String, Object> parseObjectInternal() {
        Map<String, Object> result = new HashMap<>();
        skipWhitespace();

        if (peek() != '{') {
            throw new IllegalArgumentException("Expected '{' at position " + pos);
        }
        consume(); // consume '{'

        skipWhitespace();

        if (peek() == '}') {
            consume(); // empty object
            return result;
        }

        while (true) {
            skipWhitespace();

            // Parse key
            if (peek() != '"') {
                throw new IllegalArgumentException("Expected '\"' at position " + pos);
            }
            String key = parseString();

            skipWhitespace();

            // Expect ':'
            if (peek() != ':') {
                throw new IllegalArgumentException("Expected ':' at position " + pos);
            }
            consume();

            skipWhitespace();

            // Parse value
            Object value = parseValue();
            result.put(key, value);

            skipWhitespace();

            char next = peek();
            if (next == '}') {
                consume();
                break;
            } else if (next == ',') {
                consume();
            } else {
                throw new IllegalArgumentException("Expected ',' or '}' at position " + pos);
            }
        }

        return result;
    }

    private List<Object> parseArrayInternal() {
        List<Object> result = new ArrayList<>();
        skipWhitespace();

        if (peek() != '[') {
            throw new IllegalArgumentException("Expected '[' at position " + pos);
        }
        consume(); // consume '['

        skipWhitespace();

        if (peek() == ']') {
            consume(); // empty array
            return result;
        }

        while (true) {
            skipWhitespace();
            Object value = parseValue();
            result.add(value);

            skipWhitespace();

            char next = peek();
            if (next == ']') {
                consume();
                break;
            } else if (next == ',') {
                consume();
            } else {
                throw new IllegalArgumentException("Expected ',' or ']' at position " + pos);
            }
        }

        return result;
    }

    private Object parseValue() {
        skipWhitespace();
        char c = peek();

        if (c == '"') {
            return parseString();
        } else if (c == '{') {
            return parseObjectInternal();
        } else if (c == '[') {
            return parseArrayInternal();
        } else if (c == 't' || c == 'f') {
            return parseBoolean();
        } else if (c == 'n') {
            return parseNull();
        } else if (c == '-' || Character.isDigit(c)) {
            return parseNumber();
        } else {
            throw new IllegalArgumentException("Unexpected character '" + c + "' at position " + pos);
        }
    }

    private String parseString() {
        if (peek() != '"') {
            throw new IllegalArgumentException("Expected '\"' at position " + pos);
        }
        consume(); // consume opening quote

        StringBuilder sb = new StringBuilder();

        while (true) {
            if (pos >= json.length()) {
                throw new IllegalArgumentException("Unterminated string");
            }

            char c = json.charAt(pos++);

            if (c == '"') {
                break; // end of string
            } else if (c == '\\') {
                // Escape sequence
                if (pos >= json.length()) {
                    throw new IllegalArgumentException("Unterminated escape sequence");
                }
                char escaped = json.charAt(pos++);
                switch (escaped) {
                    case '"':
                        sb.append('"');
                        break;
                    case '\\':
                        sb.append('\\');
                        break;
                    case '/':
                        sb.append('/');
                        break;
                    case 'b':
                        sb.append('\b');
                        break;
                    case 'f':
                        sb.append('\f');
                        break;
                    case 'n':
                        sb.append('\n');
                        break;
                    case 'r':
                        sb.append('\r');
                        break;
                    case 't':
                        sb.append('\t');
                        break;
                    case 'u':
                        // Unicode escape
                        if (pos + 4 > json.length()) {
                            throw new IllegalArgumentException("Invalid unicode escape");
                        }
                        String hex = json.substring(pos, pos + 4);
                        pos += 4;
                        sb.append((char) Integer.parseInt(hex, 16));
                        break;
                    default:
                        throw new IllegalArgumentException("Invalid escape sequence: \\" + escaped);
                }
            } else {
                sb.append(c);
            }
        }

        return sb.toString();
    }

    private Number parseNumber() {
        int start = pos;

        // Handle negative sign
        if (peek() == '-') {
            consume();
        }

        // Parse digits
        boolean hasDecimal = false;
        boolean hasExponent = false;

        while (pos < json.length()) {
            char c = json.charAt(pos);
            if (Character.isDigit(c)) {
                consume();
            } else if (c == '.' && !hasDecimal && !hasExponent) {
                hasDecimal = true;
                consume();
            } else if ((c == 'e' || c == 'E') && !hasExponent) {
                hasExponent = true;
                consume();
                if (pos < json.length() && (peek() == '+' || peek() == '-')) {
                    consume();
                }
            } else {
                break;
            }
        }

        String numberStr = json.substring(start, pos);

        if (hasDecimal || hasExponent) {
            return Double.parseDouble(numberStr);
        } else {
            try {
                return Long.parseLong(numberStr);
            } catch (NumberFormatException e) {
                return Double.parseDouble(numberStr);
            }
        }
    }

    private Boolean parseBoolean() {
        if (json.startsWith("true", pos)) {
            pos += 4;
            return Boolean.TRUE;
        } else if (json.startsWith("false", pos)) {
            pos += 5;
            return Boolean.FALSE;
        } else {
            throw new IllegalArgumentException("Expected boolean at position " + pos);
        }
    }

    private Object parseNull() {
        if (json.startsWith("null", pos)) {
            pos += 4;
            return null;
        } else {
            throw new IllegalArgumentException("Expected null at position " + pos);
        }
    }

    private void skipWhitespace() {
        while (pos < json.length() && Character.isWhitespace(json.charAt(pos))) {
            pos++;
        }
    }

    private char peek() {
        if (pos >= json.length()) {
            throw new IllegalArgumentException("Unexpected end of input at position " + pos);
        }
        return json.charAt(pos);
    }

    private void consume() {
        pos++;
    }

    /**
     * Helper method to get a string value from a map.
     */
    public static String getString(Map<String, Object> map, String key) {
        Object value = map.get(key);
        return value != null ? value.toString() : null;
    }

    /**
     * Helper method to get a long value from a map.
     */
    public static long getLong(Map<String, Object> map, String key) {
        Object value = map.get(key);
        if (value instanceof Number) {
            return ((Number) value).longValue();
        }
        throw new IllegalArgumentException("Expected number for key: " + key);
    }

    /**
     * Helper method to get a boolean value from a map.
     */
    public static boolean getBoolean(Map<String, Object> map, String key) {
        Object value = map.get(key);
        if (value instanceof Boolean) {
            return (Boolean) value;
        }
        throw new IllegalArgumentException("Expected boolean for key: " + key);
    }

    /**
     * Helper method to get an object map from a map.
     */
    @SuppressWarnings("unchecked")
    public static Map<String, Object> getObject(Map<String, Object> map, String key) {
        Object value = map.get(key);
        if (value instanceof Map) {
            return (Map<String, Object>) value;
        }
        throw new IllegalArgumentException("Expected object for key: " + key);
    }

    /**
     * Helper method to get an array from a map.
     */
    @SuppressWarnings("unchecked")
    public static List<Object> getArray(Map<String, Object> map, String key) {
        Object value = map.get(key);
        if (value instanceof List) {
            return (List<Object>) value;
        }
        throw new IllegalArgumentException("Expected array for key: " + key);
    }
}
