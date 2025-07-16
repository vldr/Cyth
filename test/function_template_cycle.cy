import "env"
  void log(int n)

int fibonacci<T>(int n)
  if n == 0
    return n
  else if n == 1
    return n
  else
    return fibonacci<int>(n - 2) + fibonacci<float>(n - 1)

for int i = 0; i <= 3; i = i + 1
  log(fibonacci<int>(i))

# 0
# 1
# 1
# 2