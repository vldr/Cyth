import "env"
    void log(string c)
    void log(int n)

char hello = 'a'
if hello == 'a'
    log("1")

if hello != 't'
    log("2")

char test = "test"[0]

if test == 't'
    log("1")

if test != 'a'
    log("2")

log("hello: " + hello)
log("test: " + test)

log("hello integer: " + (int)hello)
log("test integer: " + (int)test)

log((string)(char)65)
log("Character: " + (char)321)

# 1
# 2
# 1
# 2
# hello: a
# test: t
# hello integer: 97
# test integer: 116
# A
# Character: A