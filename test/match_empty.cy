class Value

int produce()
  log("produce")
  return 1

match produce()
log("after static")

any value = Value()
match value
log("after any")

match produce()
  case string value
    log("wrong")
  default
    log("default")

# after static
# after any
# default
