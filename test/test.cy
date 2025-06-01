import "env"
    void log(int n)

int fibonacci(int n)
    if not n 
        return n
    else if n == 1
        return n
    else
        return fibonacci(n - 2) + fibonacci(n - 1)

int add(int a, int b)
    if not b
        return a
    else
        return add(a + 1, b - 1)

log(fibonacci(24)) 
log(add(10, 5)) 

# 46368
# 15