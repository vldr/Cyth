any Thing = Apple

class Apple
  string name

  void __init__(string name)
    this.name = name

class Runner
  void __init__(Thing value)
    match value
      case Apple apple
        log("initializer " + apple.name)
      default
        log("initializer default")

  void run(Thing value)
    match value
      case Apple apple
        log("method " + apple.name)
      default
        log("method default")

class GenericRunner<T>
  void run(Thing value)
    match value
      case Apple apple
        log("class template " + apple.name)
      default
        log("class template default")

void inOrdinaryFunction(Thing value)
  match value
    case Apple apple
      log("function " + apple.name)
    default
      log("function default")

void inNestedFunction(Thing value)
  void nested(Thing inner)
    match inner
      case Apple apple
        log("nested function " + apple.name)
      default
        log("nested function default")

  nested(value)

void inFunctionTemplate<T>(Thing value)
  match value
    case Apple apple
      log("function template " + apple.name)
    default
      log("function template default")

void inControlFlow(Thing value)
  if true
    match value
      case Apple apple
        log("if " + apple.name)
      default
        log("if default")

  int i = 0
  while i < 1
    match value
      case Apple apple
        log("while " + apple.name)
      default
        log("while default")

    i += 1

void inNestedMatch(Thing value)
  match value
    case Apple apple
      Thing inner = apple

      match inner
        case Apple innerApple
          log("nested match " + innerApple.name)
        default
          log("nested match default")
    default
      log("outer match default")

void functionDeclaredInMatch(Thing value)
  match value
    case Apple apple
      void nestedCase(Thing inner)
        match inner
          case Apple innerApple
            log("match function " + innerApple.name)
          default
            log("match function default")

      nestedCase(apple)
    default
      log("match declaration default")

Thing topLevel = Apple("top level")
match topLevel
  case Apple apple
    log(apple.name)
  default
    log("top level default")

Thing defaultOnly = Apple("unused")
match defaultOnly
  default
    log("default only")

inOrdinaryFunction(Apple("ordinary"))
Runner runner = Runner(Apple("constructor"))
runner.run(Apple("runner"))
inNestedFunction(Apple("nested"))
inFunctionTemplate<int>(Apple("template"))
GenericRunner<int>().run(Apple("generic"))
inControlFlow(Apple("control"))
inNestedMatch(Apple("inner"))
functionDeclaredInMatch(Apple("nested declaration"))

# top level
# default only
# function ordinary
# initializer constructor
# method runner
# nested function nested
# function template template
# class template generic
# if control
# while control
# nested match inner
# match function nested declaration
