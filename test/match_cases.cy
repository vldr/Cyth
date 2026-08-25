class First
  string value = "hello world"

class Second
  int value = 42

class Other
  float value = 1.625

any first = First()
match first
  case First
    log(first.value)
  case Second
    log(first.value)

any other = Other()
match other
  case First
    log(other.value)
  case Second
    log(other.value)

log("continued")

# hello world
# continued