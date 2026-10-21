class Obj
    int field = 12
    int field2 = field--

Obj obj = Obj()
log(obj.field)
log(obj.field2)

# 11
# 12

for int i = 2; i >= 0; i--
    log(i)

# 2
# 1
# 0

int[] a = [10]
log(a[0]--)
log(a[0])

# 10
# 9

float[] b = [10.5]
log(b[0]--)
log(b[0])

# 10.5
# 9.5
