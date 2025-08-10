import "env"
    void log<T>(T n)

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

int parseInt(string n, int base)
    n = stringTrim(n)
    if not n
        return 0

    int index = 0
    bool negative = false

    if n[0] == '+'
        index += 1
    else if n[0] == '-'
        negative = true
        index += 1

    int value = 0
    while index < n.length
        char c = n[index]
        int digit

        if c >= '0' and c <= '9'
            digit = c - '0'
        else if c >= 'A' and c <= 'Z'
            digit = c - 'A' + 10
        else if c >= 'a' and c <= 'z'
            digit = c - 'a' + 10
        else
            break

        if digit >= base
            break

        value = value * base + digit
        index += 1

    if negative
        value = -value

    return value

log<int>(parseInt("-", 10))
# 0

log<int>(parseInt("+", 10))
# 0

log<int>(parseInt("", 10))
# 0

log<int>(parseInt("abcdef", 10))
# 0

log<int>(parseInt("abc10", 10))
# 0

log<int>(parseInt("10abc", 10))
# 10

log<int>(parseInt("-10abc", 10))
# -10

log<int>(parseInt("10.10", 10))
# 10

log<int>(parseInt("-10.10", 10))
# -10

log<int>(parseInt("+10.10", 10))
# 10

log<int>(parseInt("--10.10", 10))
# 0

log<int>(parseInt("++10.10", 10))
# 0

log<int>(parseInt("0", 10))
# 0

log<int>(parseInt("2147483647", 10))
# 2147483647

log<int>(parseInt("-2147483648", 10))
# -2147483648

log<int>(parseInt("-2147483650", 10))
# 2147483646

log<int>(parseInt("-2147483649", 10))
# 2147483647

log<int>(parseInt("2147483650", 10))
# -2147483646

log<int>(parseInt("2147483649", 10))
# -2147483647

log<int>(parseInt("011", 2))
# 3

log<int>(parseInt("010", 2))
# 2

log<int>(parseInt("000", 2))
# 0

log<int>(parseInt("FF", 16))
# 255

log<int>(parseInt("ff", 16))
# 255

log<int>(parseInt("F", 16))
# 15

log<int>(parseInt("f", 16))
# 15

log<int>(parseInt("g", 17))
# 16

log<int>(parseInt("ffffffff", 16))
# -1
