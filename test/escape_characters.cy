import "env"
    void log(string n)
    void log(int n)

log("\b\0\n\t\r\'\"\\")
log("\xe0\xAD\xb4")
log("Char" + '\n')
log("Char" + '\0')
log("Char" + '\b')
log("Char" + '\t')
log("Char" + '\r')
log("Char" + '\f')
log("Char" + '\'')
log("Char" + '\"')
log("Char" + '\\')
log("Char" + '\x2e')

# \b\0\n\t\r'"\
# ୴
# Char\n
# Char\0
# Char\b
# Char\t
# Char\r
# Char\f
# Char'
# Char"
# Char\
# Char.