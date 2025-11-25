import "env"
    void log<T>(T n)

int ptr = alloc(65536)
int ptr2 = alloc(1)

writeChar(ptr, 'a')
writeChar(ptr2, 'b')

log<int>(readChar(ptr))
log<int>(readChar(ptr2))

# 97
# 98

int a = alloc(11)

writeInt(a, 10)
writeFloat(a + 4, 12.5)
writeBool(a + 4 + 4, true)
writeChar(a + 4 + 4 + 1, 'h')
writeChar(a + 4 + 4 + 1 + 1, 'i')

log<int>(readInt(a))
log<float>(readFloat(a + 4))
log<bool>(readBool(a + 4 + 4))
log<int>(readChar(a + 4 + 4 + 1))
log<int>(readChar(a + 4 + 4 + 1 + 1))

# 10
# 12.5
# 1
# 104
# 105

int b = alloc(11)

writeInt(b, 0xffffffff)
writeFloat(b + 4, 9999.625)
writeBool(b + 4 + 4, false)
writeChar(b + 4 + 4 + 1, 'n')
writeChar(b + 4 + 4 + 1 + 1, 'o')

log<int>(readInt(b))
log<float>(readFloat(b + 4))
log<bool>(readBool(b + 4 + 4))
log<int>(readChar(b + 4 + 4 + 1))
log<int>(readChar(b + 4 + 4 + 1 + 1))

# -1
# 9999.625
# 0
# 110
# 111

log<int>(readInt(a))
log<float>(readFloat(a + 4))
log<bool>(readBool(a + 4 + 4))
log<int>(readChar(a + 4 + 4 + 1))
log<int>(readChar(a + 4 + 4 + 1 + 1))

# 10
# 12.5
# 1
# 104
# 105