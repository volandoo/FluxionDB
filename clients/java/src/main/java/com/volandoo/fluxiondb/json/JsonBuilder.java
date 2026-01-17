package com.volandoo.fluxiondb.json;

import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Minimal JSON builder for creating JSON strings without third-party dependencies.
 * Supports objects, arrays, and proper string escaping.
 */
public class JsonBuilder {

    private final Map<String, Object> fields = new LinkedHashMap<>();

    public JsonBuilder add(String key, String value) {
        fields.put(key, value);
        return this;
    }

    public JsonBuilder add(String key, long value) {
        fields.put(key, value);
        return this;
    }

    public JsonBuilder add(String key, int value) {
        fields.put(key, value);
        return this;
    }

    public JsonBuilder add(String key, boolean value) {
        fields.put(key, value);
        return this;
    }

    public JsonBuilder add(String key, Object value) {
        fields.put(key, value);
        return this;
    }

    public JsonBuilder addNull(String key) {
        fields.put(key, null);
        return this;
    }

    public String build() {
        StringBuilder sb = new StringBuilder();
        sb.append("{");

        boolean first = true;
        for (Map.Entry<String, Object> entry : fields.entrySet()) {
            if (!first) {
                sb.append(",");
            }
            first = false;

            sb.append("\"").append(escapeString(entry.getKey())).append("\":");
            appendValue(sb, entry.getValue());
        }

        sb.append("}");
        return sb.toString();
    }

    private void appendValue(StringBuilder sb, Object value) {
        if (value == null) {
            sb.append("null");
        } else if (value instanceof String) {
            sb.append("\"").append(escapeString((String) value)).append("\"");
        } else if (value instanceof Number || value instanceof Boolean) {
            sb.append(value);
        } else if (value instanceof Map) {
            appendObject(sb, (Map<?, ?>) value);
        } else if (value instanceof List) {
            appendArray(sb, (List<?>) value);
        } else {
            // Fallback: convert to string
            sb.append("\"").append(escapeString(value.toString())).append("\"");
        }
    }

    private void appendObject(StringBuilder sb, Map<?, ?> map) {
        sb.append("{");
        boolean first = true;
        for (Map.Entry<?, ?> entry : map.entrySet()) {
            if (!first) {
                sb.append(",");
            }
            first = false;

            sb.append("\"").append(escapeString(String.valueOf(entry.getKey()))).append("\":");
            appendValue(sb, entry.getValue());
        }
        sb.append("}");
    }

    private void appendArray(StringBuilder sb, List<?> list) {
        sb.append("[");
        boolean first = true;
        for (Object item : list) {
            if (!first) {
                sb.append(",");
            }
            first = false;
            appendValue(sb, item);
        }
        sb.append("]");
    }

    /**
     * Escapes special characters in JSON strings.
     */
    private String escapeString(String s) {
        if (s == null) {
            return "";
        }

        StringBuilder sb = new StringBuilder(s.length());
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            switch (c) {
                case '\\':
                    sb.append("\\\\");
                    break;
                case '"':
                    sb.append("\\\"");
                    break;
                case '\b':
                    sb.append("\\b");
                    break;
                case '\f':
                    sb.append("\\f");
                    break;
                case '\n':
                    sb.append("\\n");
                    break;
                case '\r':
                    sb.append("\\r");
                    break;
                case '\t':
                    sb.append("\\t");
                    break;
                default:
                    if (c < ' ') {
                        String hex = Integer.toHexString(c);
                        sb.append("\\u");
                        for (int j = 0; j < 4 - hex.length(); j++) {
                            sb.append('0');
                        }
                        sb.append(hex);
                    } else {
                        sb.append(c);
                    }
            }
        }
        return sb.toString();
    }

    /**
     * Builds a JSON array from a list of objects.
     */
    public static String toJsonArray(List<?> list) {
        StringBuilder sb = new StringBuilder();
        sb.append("[");
        boolean first = true;
        for (Object item : list) {
            if (!first) {
                sb.append(",");
            }
            first = false;

            if (item instanceof String) {
                sb.append("\"").append(new JsonBuilder().escapeString((String) item)).append("\"");
            } else if (item instanceof Number || item instanceof Boolean) {
                sb.append(item);
            } else if (item instanceof Map) {
                new JsonBuilder().appendObject(sb, (Map<?, ?>) item);
            } else {
                sb.append(item.toString());
            }
        }
        sb.append("]");
        return sb.toString();
    }
}
