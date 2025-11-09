import "env"
    void log(string n)

bool stringContains(string s, string target)
    return s.indexOf(target) != -1

string stringTrim(string s)
    if not s
        return s

    int start = 0
    int end = s.length - 1

    while start < s.length and (s[start] == ' ' or s[start] == '\t' or s[start] == '\n' or s[start] == '\r')
        start += 1

    while end >= start and (s[end] == ' ' or s[end] == '\t' or s[end] == '\n' or s[end] == '\r')
        end -= 1

    char[] result
    for int i = start; i <= end; i += 1
        result.push(s[i])

    return result.toString()

string stringJoin(string[] parts, string delim)
    string[] result
    char[] buf

    for string part in parts
        for char c in part
            buf.push(c)

        if it != parts.length - 1
            for char c in delim
                buf.push(c)

    return buf.toString()

log("Empty" + stringTrim(""))
log("Contains " + stringContains("", ""))
log("Contains " + stringContains("abc", ""))
log("Contains " + stringContains("", "abc"))
log("IndexOf " + "".indexOf(""))
log("IndexOf " + "abc".indexOf(""))
log("IndexOf " + "".indexOf("abc"))
log("Length " + "".split('\\').length)

# Empty
# Contains true
# Contains true
# Contains false
# IndexOf 0
# IndexOf 0
# IndexOf -1
# Length 1

string[] chunks = "hello, world, how are you".split(',')
for string chunk in chunks
    log(stringTrim(chunk))

# hello
# world
# how are you

log(stringJoin(chunks, ","))
log("" + stringJoin(chunks, ",").indexOf("world"))
log("" + stringJoin(chunks, ",").indexOf("space"))
log("" + stringJoin(chunks, ",").indexOf(""))

# hello, world, how are you
# 7
# -1
# 0

chunks = "hello |   world   |         how are you".split('|')
for string chunk in chunks
    log(stringTrim(chunk))

# hello
# world
# how are you

log(stringJoin(chunks, "|"))
log("" + stringJoin(chunks, "|").indexOf("you"))
log("" + stringJoin(chunks, "|").indexOf("space"))
log("" + stringJoin(chunks, "|").indexOf(""))

# hello |   world   |         how are you
# 36
# -1
# 0

log("" + "".indexOf("space"))

# -1