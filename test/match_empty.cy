class Value

any Thing = Value

Thing produce()
  log("produce")
  return null

match produce()
  default
log("after static")

Thing value = Value()
match value
  default
log("after any")

match produce()
  case Value value
    log("wrong")
  default
    log("default")

# after static
# after any
# produce
# default
