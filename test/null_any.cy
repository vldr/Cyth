class Obj
    int field

void foo(Thing value)
    match value
        case Obj
            log(value.field)

any Thing = Obj

Obj apple
foo(apple)