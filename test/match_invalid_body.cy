class First
class Second

any value = First()
match value
  log("before one")
  log("before two")
  case First
    log("first")
  log("between one")
  log("between two")
  case Second
    log("second")
  log("after one")
  log("after two")

#! 6:3-7:21 Expected 'case' or 'default' inside match.
#! 10:3-11:22 Expected 'case' or 'default' inside match.
#! 14:3-15:20 Expected 'case' or 'default' inside match.
