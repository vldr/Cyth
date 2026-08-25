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


any result = Result()
any other = Other()

log(complete(result))
log(complete(other))
log(returnsAfterMatch(result))
log(returnsAfterMatch(other))

# 1
# 2
# 3
# 4
