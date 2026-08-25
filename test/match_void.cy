void voidFunction()
  log("void function")

match voidFunction()
  default

#! 4:1-4:6 Cannot match on 'void', only 'any' and template types are supported.
