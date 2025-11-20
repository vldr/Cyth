import "env"
    void log(int n)
    void log(string n)
    void log(bool n)

class Apple
    int field = 33

class Apple2
    int field = 33

any[] test = ["hello", Apple(), (int[])[1, 2, 3]]

string a = (string)test[0]
Apple b = (Apple)test[1]
int[] c = (int[])test[2]

log(a)
log(b.field)
log(c[0])
log(c[1])
log(c[2])

# hello
# 33
# 1
# 2
# 3

log(test[0] is string)
log(test[1] is string)
log(test[2] is string)

# 1
# 0
# 0

log(test[0] is Apple)
log(test[1] is Apple)
log(test[2] is Apple)

# 0
# 1
# 0

log(test[0] is int[])
log(test[1] is int[])
log(test[2] is int[])

# 0
# 0
# 1