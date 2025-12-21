import "env"
    void log(int n)
    void log(string n)

int[] a

a = [1,2,3]
for int i = 0; i < a.length; i += 1
    log((string)a)

    a.remove(i)
    i -= 1

a = [1, 2, 3]
for int i = a.length - 1; i >= 0; i -= 1
    log((string)a)

    a.remove(i)

a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

while a.length
    log((string)a)
    a.remove(a.length / 2)

log((string)a)

# [1, 2, 3]
# [2, 3]
# [3]
# [1, 2, 3]
# [1, 2]
# [1]
# [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
# [1, 2, 3, 4, 5, 7, 8, 9, 10]
# [1, 2, 3, 4, 7, 8, 9, 10]
# [1, 2, 3, 4, 8, 9, 10]
# [1, 2, 3, 8, 9, 10]
# [1, 2, 3, 9, 10]
# [1, 2, 9, 10]
# [1, 2, 10]
# [1, 10]
# [1]
# []