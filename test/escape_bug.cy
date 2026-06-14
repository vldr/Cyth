class Test
  int value

void test(int n, int target)
  Test result

  for int i = 0; i < n; i += 1
    Test p = Test()
    p.value = i * 10

    if i == target
      result = p

  log(result.value)

test(10, 5)

# 50