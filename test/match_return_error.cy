class Result

int noDefault(any value)
  match value
    case Result result
      return 1

int incompleteDefault(any value)
  match value
    case Result result
      return 2
    default
      log("missing return")

#! 3:5-3:14 Non-void function must return a value.
#! 8:5-8:22 Non-void function must return a value.
