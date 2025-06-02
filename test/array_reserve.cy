import "env"
    void log(int n)

string[] a
a.reserve(10)

log(a.length)

# 10

string[][] b
b.reserve(10, 20)

log(b.length)
log(b[0].length)

# 10
# 20

string[][][] c
c.reserve(10, 20, 30)

log(c.length)
log(c[0].length)
log(c[0][0].length)

# 10
# 20
# 30

string[][][] d
d.reserve(0, 20, 30)

log(d.length)

# 0

string[][][] e
e.reserve(10, 0, 30)

log(e.length)
log(e[0].length)

# 10
# 0

string[][][] f
f.reserve(10, 20, 0)

log(f.length)
log(f[0].length)
log(f[0][0].length)

# 10
# 20
# 0