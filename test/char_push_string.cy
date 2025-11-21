import "env"
    void log(string n)
    void log(int n)

char[] a
log("Length " + a.length + " Capacity " + a.capacity)

# Length 0 Capacity 0

a.reserve(3)
log("Length " + a.length + " Capacity " + a.capacity)

# Length 3 Capacity 3

a.pushString("")
log("Length " + a.length + " Capacity " + a.capacity)

# Length 3 Capacity 6

a.pushString("hello")
log("Length " + a.length + " Capacity " + a.capacity)

# Length 8 Capacity 16

a.pushString("world")
log("Length " + a.length + " Capacity " + a.capacity)

# Length 13 Capacity 16

a.pushString("hej")
log("Length " + a.length + " Capacity " + a.capacity)

# Length 16 Capacity 32

for char a in a
    if a == '\0'
        log("␀")
    else
        log((string)a)

# ␀
# ␀
# ␀

# h
# e
# l
# l
# o

# w
# o
# r
# l
# d

# h
# e
# j
