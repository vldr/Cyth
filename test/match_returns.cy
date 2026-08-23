class Result
  int value

class Other
  string value

int complete(any value)
  match value
    case Result result
      return 1
    default
      return 2

int returnsAfterMatch(any value)
  match value
    case Result result
      return 3

  return 4

int completeStatic(int value)
  match value
    case int matched
      return matched
    default
      return -1

any result = Result()
any other = Other()

log(complete(result))
log(complete(other))
log(returnsAfterMatch(result))
log(returnsAfterMatch(other))
log(completeStatic(5))

# 1
# 2
# 3
# 4
# 5
