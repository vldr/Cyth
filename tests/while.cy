import "env"
    void log(int n)

int fib(int n)
    int a
    int b
    int c = 1
    int i = 1

    while i < n
        a = b
        b = c
        c = a + b
        i += 1

    return c

log(fib(42))

# 267914296