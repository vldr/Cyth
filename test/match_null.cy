class Value

any nullValue

match nullValue
  case Value value
    log("wrong")
  default
    log("default")

match nullValue
  case Value value
    log("wrong")

log("continued")

# default
# continued
