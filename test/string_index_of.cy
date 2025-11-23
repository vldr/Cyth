import "env"
    void log(string n)
    void log(int n)

log("Empty" + "".trim())
log("Contains " + "".contains(""))
log("Contains " + "abc".contains(""))
log("Contains " + "".contains("abc"))
log("IndexOf " + "".indexOf(""))
log("IndexOf " + "abc".indexOf(""))
log("IndexOf " + "".indexOf("abc"))
log("Length " + "".split("\\").length)

# Empty
# Contains true
# Contains true
# Contains false
# IndexOf 0
# IndexOf 0
# IndexOf -1
# Length 1

string[] chunks = "hello, world, how are you".split(",")
for string chunk in chunks
    log(chunk.trim())

# hello
# world
# how are you

log(chunks.join(","))
log("" + chunks.join(",").indexOf("world"))
log("" + chunks.join(",").indexOf("space"))
log("" + chunks.join(",").indexOf(""))

# hello, world, how are you
# 7
# -1
# 0

chunks = "hello |   world   |         how are you".split("|")
for string chunk in chunks
    log(chunk.trim())

# hello
# world
# how are you

log(chunks.join("|"))
log("" + chunks.join("|").indexOf("you"))
log("" + chunks.join("|").indexOf("space"))
log("" + chunks.join("|").indexOf(""))

# hello |   world   |         how are you
# 36
# -1
# 0

log("" + "".indexOf("space"))

# -1