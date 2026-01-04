import "env"
    void log(string n)
    void log(int n)

log("" + 123.625)
log("" + 21487.625)
log("" + 4294967296.625)

log("" + inf)
log("" + -inf)

log("" + true)
log("" + false)

log("" + 2147483647)
log("" + -2147483648)

for int i = -10; i <= 10; i += 1
    log("" + i)

# 123.625
# 21487.625
# 4294967296
# inf
# -inf
# true
# false
# 2147483647
# -2147483648
# -10
# -9
# -8
# -7
# -6
# -5
# -4
# -3
# -2
# -1
# 0
# 1
# 2
# 3
# 4
# 5
# 6
# 7
# 8
# 9
# 10

log((string)123.625)
log((string)21487.625)
log((string)4294967296.625)

log((string)inf)
log((string)-inf)

log((string)true)
log((string)false)

log((string)2147483647)
log((string)-2147483648)

for int i = -10; i <= 10; i += 1
    log((string)i)

# 123.625
# 21487.625
# 4294967296
# inf
# -inf
# true
# false
# 2147483647
# -2147483648
# -10
# -9
# -8
# -7
# -6
# -5
# -4
# -3
# -2
# -1
# 0
# 1
# 2
# 3
# 4
# 5
# 6
# 7
# 8
# 9
# 10