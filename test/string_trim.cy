import "env"
    void log(string n)
    void log(int n)

log("   Hello   ".trim())
# Hello

log("\tHello\n".trim())
# Hello

log("\n\t  Hello  \t\n".trim())
# Hello

log("\r\rHello\r\r".trim())
# Hello

log(" \t\n\rHello \t\n\r".trim())
# Hello

log("Hello".trim())
# Hello

log("    ".trim())
# 

log("\n\t\r".trim())
# 

log("   Hello   World   ".trim())
# Hello   World

log("\t\n  A  B  C  \r\n\t".trim())
# A  B  C

log("".trim())
# 

log("     ".trim())
# 

log("\t\t\t".trim())
# 

log("\n\n".trim())
# 

log("\r\r\r".trim())
# 

log(" \t\n\r \t\n\r ".trim())
# 

log(" \t\n\rA\r\n\t ".trim())
# A

log("   Hello".trim())
# Hello

log("Hello   ".trim())
# Hello

log("  Hello   World  ".trim())
# Hello   World

log("Hello".trim())
# Hello

log("Hello World".trim())
# Hello World
