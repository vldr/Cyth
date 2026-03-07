class B<T>
    T field2 = global

class A<T>
    T field = B<T>().field2

string global = "hello"
A<string> a = A<string>()
log(a.field)

# hello