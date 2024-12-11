int fibonacci(int n)
    if n == 0
        return n
    else if n == 1
        return n
    else
        return fibonacci(n - 2) + fibonacci(n - 1)

int add(int a, int b)
    if b == 0
        return a
    else
        return add(a + 1, b - 1)