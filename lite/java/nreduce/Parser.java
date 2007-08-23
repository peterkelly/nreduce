package nreduce;

import java.util.List;
import java.util.ArrayList;

public class Parser
{
    public static String[] parse(String line)
    {
        char[] chars = line.toCharArray();
        final int STATE_NORMAL = 0;
        final int STATE_SPACE = 1;
        final int STATE_STRING = 2;
        final int STATE_ESCAPED = 3;
        int state = STATE_NORMAL;
        int start = 0;
        int pos = 0;
        List<String> args = new ArrayList<String>(20);
        while (chars.length > pos) {
            char c = chars[pos];
            switch (state) {
            case STATE_NORMAL:
                if (Character.isWhitespace(c)) {
                    args.add(line.substring(start,pos));
                    state = STATE_SPACE;
                }
                break;
            case STATE_SPACE:
                if (!Character.isWhitespace(c)) {
                    start = pos;
                    if ('"' == c)
                        state = STATE_STRING;
                    else
                        state = STATE_NORMAL;
                }
                break;
            case STATE_STRING:
                if ('"' == c) {
                    args.add(line.substring(start,pos+1));
                    state = STATE_SPACE;
                }
                else if ('\\' == c) {
                    state = STATE_ESCAPED;
                }
                break;
            case STATE_ESCAPED:
                state = STATE_STRING;
                break;
            }
            pos++;
        }
        if (STATE_NORMAL == state)
            args.add(line.substring(start,pos));
        return args.toArray(new String[]{});
    }

    public static String unescape(String s)
    {
        char[] chars = s.toCharArray();
        char[] unescaped = new char[chars.length];
        int from = 0;
        int to = 0;
        while (from < chars.length) {
            if ('\\' == chars[from]) {
                from++;
                switch (chars[from]) {
                case 'b': unescaped[to++] = '\b'; break;
                case 't': unescaped[to++] = '\t'; break;
                case 'n': unescaped[to++] = '\n'; break;
                case 'f': unescaped[to++] = '\f'; break;
                case 'r': unescaped[to++] = '\r'; break;
                case '"': unescaped[to++] = '\"'; break;
                case '\'': unescaped[to++] = '\''; break;
                case '\\': unescaped[to++] = '\\'; break;
                }
                from++;
            }
            else {
                unescaped[to++] = chars[from++];
            }
        }
        return new String(unescaped,0,to);
    }

    public static String escape(String s)
    {
        char[] chars = s.toCharArray();
        char[] escaped = new char[chars.length*2];
        int from = 0;
        int to = 0;
        while (from < chars.length) {
            switch (chars[from]) {
            case '\b':
                escaped[to++] = '\\';
                escaped[to++] = 'b';
                break;
            case '\t':
                escaped[to++] = '\\';
                escaped[to++] = 't';
                break;
            case '\n':
                escaped[to++] = '\\';
                escaped[to++] = 'n';
                break;
            case '\f':
                escaped[to++] = '\\';
                escaped[to++] = 'f';
                break;
            case '\r':
                escaped[to++] = '\\';
                escaped[to++] = 'r';
                break;
            case '\"':
                escaped[to++] = '\\';
                escaped[to++] = '"';
                break;
            case '\'':
                escaped[to++] = '\\';
                escaped[to++] = '\'';
                break;
            case '\\':
                escaped[to++] = '\\';
                escaped[to++] = '\\';
                break;
            default:
                escaped[to++] = chars[from];
                break;
            }
            from++;
        }
        return new String(escaped,0,to);
    }

}
