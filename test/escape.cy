for string a in ("hello\
world".split("\n"))
    log(a.replace("\r", ""))

# hello
# world

log("\g")
# g