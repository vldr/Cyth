class Result
class Other

any Thing = Result | Other

int complete(Thing value)
  match value
    case Result result
      return 1
    default
      return 2

int returnsAfterMatch(Thing value)
  match value
    case Result result
      return 3
    default

  return 4


Thing result = Result()
Thing other = Other()

log(complete(result))
log(complete(other))
log(returnsAfterMatch(result))
log(returnsAfterMatch(other))

# 1
# 2
# 3
# 4
