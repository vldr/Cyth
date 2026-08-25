
void func<T>()
  T source = 10

  match T
    case int
      source = 50
    case float
      source = 12.625

  log(source)


func<int>()
func<float>()

# 50
# 12.625