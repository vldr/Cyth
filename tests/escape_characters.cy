import "env"
    void log(string n)

log("\b\0\n\t\r\'\"\\")
log("Char" + (string)'\n')
log("Char" + (string)'\0')
log("Char" + (string)'\b')
log("Char" + (string)'\t')
log("Char" + (string)'\r')
log("Char" + (string)'\f')
log("Char" + (string)'\'')
log("Char" + (string)'\"')
log("Char" + (string)'\\')

# \b\0\n\t\r'"\
# Char\n
# Char\0
# Char\b
# Char\t
# Char\r
# Char\f
# Char'
# Char"
# Char\