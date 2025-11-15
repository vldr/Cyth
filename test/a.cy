
import "env"
    void log(int n)

int fibonacci(int n)
  if n == 0
    return n
  else if n == 1
    return n
  else
    return fibonacci(n - 2) + fibonacci(n - 1)

for int i = 0; i <= 42; i += 1
  log(fibonacci(i))
