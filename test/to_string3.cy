import "env"
    void log(string n)
    void log(int n)

void not_overload()

any a = (any)null

string test<T>()
    return (string)T + " " + T

class Object
    int field = 12

    void test(int test)

class Object2<T>
    int field = 12

Object b = (Object)null
void(string) c = (void(string))null

log((string)b)
log((string)c)

# null
# null

log("" + b)
log("" + c)

# null
# null

b = Object()
c = (void(string))log

log((string)b)
log((string)c)

# Object(\n field = 12\n)
# void(string)

log("" + b)
log("" + c)

# Object(\n field = 12\n)
# void(string)

log((string)log)
log((string)b.test)
log((string)test)
log((string)"".toArray)
log(test<string>())
log((string)not_overload)

# void(string), void(int)
# void(Object, int)
# string<T>()
# char[](string)
# string string
# void()

log("" + log)
log("" + b.test)
log("" + test)
log("" + "".toArray)
log("" + not_overload)

# void(string), void(int)
# void(Object, int)
# string<T>()
# char[](string)
# void()

log((string)Object)
log((string)Object2)
log((string)Object2<int>)

# class Object
# class Object2<T>
# class Object2<int>

log("" + Object)
log("" + Object2)
log("" + Object2<int>)

# class Object
# class Object2<T>
# class Object2<int>
