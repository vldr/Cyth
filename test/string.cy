import "env"
    void log(string n)

int stringIndexOf(string s, string target)
    if target.length == 0
        return 0

    for int i = 0; i <= s.length - target.length; i += 1
        bool match = true

        for char c in target
            if s[i + it] != c
                match = false
                break

        if match
            return i

    return -1

bool stringContains(string s, string target)
    return stringIndexOf(s, target) != -1

string stringTrim(string s)
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

string[] stringSplit(string s, char delim)
    string[] result
    char[] current

    for char c in s
        if c != delim
            current.push(c)
        else
            result.push(current.toString())
            current.clear()

    if current.length
        result.push(current.toString())

    return result

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

string[] chunks = stringSplit("hello, world, how are you", ',')
for string chunk in chunks
    log(stringTrim(chunk))

# hello
# world
# how are you

log(stringJoin(chunks, ","))
log("" + stringIndexOf(stringJoin(chunks, ","), "world"))
log("" + stringIndexOf(stringJoin(chunks, ","), "space"))
log("" + stringIndexOf(stringJoin(chunks, ","), ""))

# hello, world, how are you
# 7
# -1
# 0

chunks = stringSplit("hello |   world   |         how are you", '|')
for string chunk in chunks
    log(stringTrim(chunk))

# hello
# world
# how are you

log(stringJoin(chunks, "|"))
log("" + stringIndexOf(stringJoin(chunks, "|"), "you"))
log("" + stringIndexOf(stringJoin(chunks, "|"), "space"))
log("" + stringIndexOf(stringJoin(chunks, "|"), ""))

# hello |   world   |         how are you
# 36
# -1
# 0

log("" + stringIndexOf("", "space"))

# -1