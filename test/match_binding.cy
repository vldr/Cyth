int source = 10

match source
  case int binding
    binding = 20
    log(binding)

log(source)

match source
  case int
    source = 50
    log(source)

log(source)

# 20
# 10
# 50
# 10
