import "env"
    void log(int n)
    void log(string n)

int[] a

a = [1,2,3]
for int i = 0; i < a.length; i += 1
    log((string)a)
    log(a.remove(i))
    i -= 1

a = [1, 2, 3]
for int i = a.length - 1; i >= 0; i -= 1
    log((string)a)
    log(a.remove(i))

a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

while a.length
    log((string)a)
    log(a.remove(a.length / 2))

log((string)a)

# [1, 2, 3]
# 1
# [2, 3]
# 2
# [3]
# 3
# [1, 2, 3]
# 3
# [1, 2]
# 2
# [1]
# 1
# [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
# 6
# [1, 2, 3, 4, 5, 7, 8, 9, 10]
# 5
# [1, 2, 3, 4, 7, 8, 9, 10]
# 7
# [1, 2, 3, 4, 8, 9, 10]
# 4
# [1, 2, 3, 8, 9, 10]
# 8
# [1, 2, 3, 9, 10]
# 3
# [1, 2, 9, 10]
# 9
# [1, 2, 10]
# 2
# [1, 10]
# 10
# [1]
# 1
# []