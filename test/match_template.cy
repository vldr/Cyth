class Box<T>
  T value

  void __init__(T value)
    this.value = value

class TemplateMatcher<T>
  int matches(T value)
    match value
      case int
        return 1
      case float
        return 2
      case string
        return 3
      case int[]
        return 4
      case Box<int>
        return 5
      default
        return 0

int matches<T>(T value)
  match value
    case int matched
      return 1
    case float matched
      return 2
    case string matched
      return 3
    case int[] matched
      return 4
    case Box<int> matched
      return 5
    default
      return 0

log(matches<int>(10))
log(matches<float>(20.0))
log(matches<string>("string"))
log(matches<int[]>((int[])[1, 2, 3]))
log(matches<Box<int>>(Box<int>(30)))
log(matches<bool>(true))

# 1
# 2
# 3
# 4
# 5
# 0

log(TemplateMatcher<int>().matches(40))
log(TemplateMatcher<float>().matches(50.0))
log(TemplateMatcher<string>().matches("string"))
log(TemplateMatcher<int[]>().matches((int[])[4, 5, 6]))
log(TemplateMatcher<Box<int>>().matches(Box<int>(60)))
log(TemplateMatcher<bool>().matches(true))

# 1
# 2
# 3
# 4
# 5
# 0
