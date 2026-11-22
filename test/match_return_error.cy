class Result
any Thing = Result

Thing a

int noDefault(Thing value)
  match value
    case Result result
      return 1

int incompleteDefault(Thing value)
  match value
    case Result result
      return 2
    default
      log("missing return")

#! 6:5-6:14 Non-void function must return a value.
#! 11:5-11:22 Non-void function must return a value.
