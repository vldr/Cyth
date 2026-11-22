class Value 

any Thing = Value
Thing nullValue

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
