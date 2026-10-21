class Obj
    int field = 12
    int field2 = ++field

Obj obj = Obj()
log(obj.field)
log(obj.field2)

# 13
# 13

for int i; i < 3; ++i
    log(i)

# 0
# 1
# 2

int[] a = [10]
log(++a[0])
log(a[0])

# 11
# 11

float[] b = [10.5]
log(++b[0])
log(b[0])

# 11.5
# 11.5
