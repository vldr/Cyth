class Obj
    int field

void foo(any value)
    match value
        case Obj
            log(value.field)

Obj apple
foo(apple)