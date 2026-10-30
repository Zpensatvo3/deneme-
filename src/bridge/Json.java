package net.onestep.hybrid;

import java.math.BigDecimal;
import java.nio.ByteBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Strict bounded UTF-8 codec shared by public config, keybinds and the native protocol. */
public final class Json {
    private Json() {}
    public static final int MAX_BYTES = 96 * 1024;
    public static Object parse(byte[] bytes) {
        return parseBytes(bytes, MAX_BYTES);
    }
    public static Object protocol(byte[] bytes) { return parseBytes(bytes, MAX_BYTES * 6 + 4096); }
    private static Object parseBytes(byte[] bytes, int limit) {
        if (bytes == null || bytes.length == 0 || bytes.length > limit) throw error("JSON size limit or empty document");
        try {
            return parse(StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString(), limit);
        } catch (CharacterCodingException e) { throw error("Invalid UTF-8"); }
    }
    public static Object parse(String text) {
        return parse(text, MAX_BYTES);
    }
    private static Object parse(String text, int limit) {
        if (text == null || text.isEmpty() || text.length() > limit || text.getBytes(StandardCharsets.UTF_8).length > limit)
            throw error("JSON exceeds 96 KB or is empty");
        Parser parser = new Parser(text);
        Object result = parser.read(0);
        parser.spaces();
        if (parser.position != text.length()) throw error("Trailing JSON data");
        return result;
    }
    @SuppressWarnings("unchecked")
    public static Map<String, Object> object(Object value) {
        if (!(value instanceof Map)) throw error("Expected object");
        return (Map<String, Object>) value;
    }
    @SuppressWarnings("unchecked")
    public static List<Object> array(Object value) {
        if (!(value instanceof List)) throw error("Expected array");
        return (List<Object>) value;
    }
    public static int integer(Object value) {
        if (!(value instanceof BigDecimal number) || number.scale() > 0) throw error("Expected integer");
        try { return number.intValueExact(); } catch (ArithmeticException e) { throw error("Integer overflow"); }
    }
    public static boolean bool(Object value) {
        if (!(value instanceof Boolean result)) throw error("Expected boolean");
        return result;
    }
    public static String string(Object value) {
        if (!(value instanceof String result)) throw error("Expected string");
        return result;
    }
    public static IllegalArgumentException error(String message) { return new IllegalArgumentException(message); }
    public static byte[] bytes(Object value) { return stringify(value).getBytes(StandardCharsets.UTF_8); }
    public static String stringify(Object value) {
        StringBuilder output = new StringBuilder(); write(output, value); return output.toString();
    }
    private static void write(StringBuilder out, Object value) {
        if (value == null) out.append("null");
        else if (value instanceof String text) {
            out.append('"');
            for (int i = 0; i < text.length(); ++i) {
                char c = text.charAt(i);
                if (c == '"' || c == '\\') out.append('\\').append(c);
                else if (c < 32) out.append(String.format(java.util.Locale.ROOT, "\\u%04x", (int)c));
                else out.append(c);
            }
            out.append('"');
        } else if (value instanceof Map<?, ?> map) {
            out.append('{'); boolean first = true;
            for (var entry : map.entrySet()) {
                if (!first) out.append(','); first = false;
                write(out, entry.getKey()); out.append(':'); write(out, entry.getValue());
            }
            out.append('}');
        } else if (value instanceof List<?> list) {
            out.append('['); boolean first = true;
            for (Object item : list) { if (!first) out.append(','); first = false; write(out, item); }
            out.append(']');
        } else if (value instanceof Boolean || value instanceof BigDecimal || value instanceof Integer || value instanceof Long)
            out.append(value);
        else throw error("Unsupported JSON value");
    }
    private static final class Parser {
        final String text; int position, nodes;
        Parser(String text) { this.text = text; }
        void spaces() { while (position < text.length() && " \t\r\n".indexOf(text.charAt(position)) >= 0) ++position; }
        boolean take(char c) { if (position < text.length() && text.charAt(position) == c) { ++position; return true; } return false; }
        void require(char c) { if (!take(c)) throw error("Expected '" + c + "'"); }
        Object read(int depth) {
            if (depth > 32 || ++nodes > 16384) throw error("JSON complexity limit");
            spaces();
            if (position == text.length()) throw error("Incomplete JSON");
            if (take('{')) {
                Map<String, Object> map = new LinkedHashMap<>(); spaces();
                if (!take('}')) do {
                    spaces(); String key = quoted(); spaces(); require(':');
                    if (map.containsKey(key)) throw error("Duplicate JSON field: " + key);
                    map.put(key, read(depth + 1)); spaces();
                    if (take('}')) return Collections.unmodifiableMap(map);
                    require(',');
                } while (true);
                return Collections.unmodifiableMap(map);
            }
            if (take('[')) {
                List<Object> list = new ArrayList<>(); spaces();
                if (!take(']')) do {
                    list.add(read(depth + 1)); spaces();
                    if (take(']')) return Collections.unmodifiableList(list);
                    require(',');
                } while (true);
                return Collections.unmodifiableList(list);
            }
            if (text.charAt(position) == '"') return quoted();
            for (String word : List.of("true", "false", "null")) if (text.startsWith(word, position)) {
                position += word.length(); return word.equals("null") ? null : word.equals("true");
            }
            int start = position; take('-');
            if (!take('0')) { if (!digit()) throw error("Invalid number"); while (digit()) ++position; }
            if (take('.')) { if (!digit()) throw error("Invalid fraction"); while (digit()) ++position; }
            boolean exponent = take('e') || take('E');
            if (exponent) { if (!take('+')) take('-'); if (!digit()) throw error("Invalid exponent"); while (digit()) ++position; }
            String raw = text.substring(start, position);
            // Keep lexical integer compatibility: 1.0 / 1e0 are not keybinds or enums.
            try {
                BigDecimal number = new BigDecimal(raw);
                if (exponent && number.scale() <= 0) number = number.setScale(1);
                return number;
            } catch (NumberFormatException | ArithmeticException e) { throw error("Invalid number"); }
        }
        boolean digit() { return position < text.length() && text.charAt(position) >= '0' && text.charAt(position) <= '9'; }
        int hex() {
            int result = 0;
            for (int i = 0; i < 4; ++i) {
                if (position == text.length()) throw error("Incomplete Unicode escape");
                int digit = Character.digit(text.charAt(position++), 16);
                if (digit < 0) throw error("Invalid Unicode escape"); result = result * 16 + digit;
            }
            return result;
        }
        String quoted() {
            require('"'); StringBuilder result = new StringBuilder();
            while (position < text.length()) {
                char c = text.charAt(position++);
                if (c == '"') return result.toString();
                if (c < 32) throw error("Unescaped control character");
                if (c == '\\') {
                    if (position == text.length()) throw error("Incomplete escape");
                    c = text.charAt(position++);
                    switch (c) {
                        case '"', '\\', '/' -> result.append(c);
                        case 'b' -> result.append('\b'); case 'f' -> result.append('\f');
                        case 'n' -> result.append('\n'); case 'r' -> result.append('\r'); case 't' -> result.append('\t');
                        case 'u' -> {
                            char high = (char)hex();
                            if (Character.isHighSurrogate(high)) {
                                require('\\'); require('u'); char low = (char)hex();
                                if (!Character.isLowSurrogate(low)) throw error("Invalid surrogate pair");
                                result.append(high).append(low);
                            } else { if (Character.isLowSurrogate(high)) throw error("Unpaired surrogate"); result.append(high); }
                        }
                        default -> throw error("Invalid string escape");
                    }
                } else if (Character.isHighSurrogate(c)) {
                    if (position == text.length() || !Character.isLowSurrogate(text.charAt(position))) throw error("Unpaired surrogate");
                    result.append(c).append(text.charAt(position++));
                } else { if (Character.isLowSurrogate(c)) throw error("Unpaired surrogate"); result.append(c); }
            }
            throw error("Unterminated string");
        }
    }
}
