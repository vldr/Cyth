void test(int a, int b, int c, int d, int e, int f, int g, int h, any i, char j, char k, int l)
    log(a)
    log(b)
    log(c)
    log(d)
    log(e)
    log(f)
    log(g)
    log(h)
    log(i == null)
    log((string)j)
    log((string)k)
    log(l)

log(1, 2, 3, 4, 5, 6, 7, 8, null, 'A', 'B', 12345678)
# 1
# 2
# 3
# 4
# 5
# 6
# 7
# 8
# 1
# A
# B
# 12345678

log(test)
# 1
# 2
# 3
# 4
# 5
# 6
# 7
# 8
# 1
# A
# B
# 12345678