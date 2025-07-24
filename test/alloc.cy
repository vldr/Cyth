import "env"
    void log<T>(T n)

int a = alloc(11)

writeInt(a, 10)
writeFloat(a + 4, 12.5)
writeBool(a + 4 + 4, true)
writeChar(a + 4 + 4 + 1, 'h')
writeChar(a + 4 + 4 + 1 + 1, 'i')

log<int>(readInt(a))
log<float>(readFloat(a + 4))
log<bool>(readBool(a + 4 + 4))
log<char>(readChar(a + 4 + 4 + 1))
log<char>(readChar(a + 4 + 4 + 1 + 1))

# 10
# 12.5
# 1
# 104
# 105

int b = alloc(11)

log<string>("address " + b)

# address 12

writeInt(b, 0xffffffff)
writeFloat(b + 4, 9999.625)
writeBool(b + 4 + 4, false)
writeChar(b + 4 + 4 + 1, 'n')
writeChar(b + 4 + 4 + 1 + 1, 'o')

log<int>(readInt(b))
log<float>(readFloat(b + 4))
log<bool>(readBool(b + 4 + 4))
log<char>(readChar(b + 4 + 4 + 1))
log<char>(readChar(b + 4 + 4 + 1 + 1))

# -1
# 9999.625
# 0
# 110
# 111

log<int>(readInt(a))
log<float>(readFloat(a + 4))
log<bool>(readBool(a + 4 + 4))
log<char>(readChar(a + 4 + 4 + 1))
log<char>(readChar(a + 4 + 4 + 1 + 1))

# 10
# 12.5
# 1
# 104
# 105

allocReset()

int c = alloc(11)

log<string>("address " + c)

# address 0

log<int>(readInt(c))
log<float>(readFloat(c + 4))
log<bool>(readBool(c + 4 + 4))
log<char>(readChar(c + 4 + 4 + 1))
log<char>(readChar(c + 4 + 4 + 1 + 1))

# 10
# 12.5
# 1
# 104
# 105