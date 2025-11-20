import "env"
    void log(string a)
    void log(int n)

class Printer<T>
    void print(T a)
        for int i = 0; i < a.length; i += 1
            log((string)a[i])


int[] a = [ 1, 2, 3 ]
string[][] b = [ [], [ "hello", "world" ] ]

Printer<int[]>().print(a)
Printer<string[]>().print(b[0])
Printer<string[]>().print(b[1])

# 1
# 2
# 3

# hello
# world