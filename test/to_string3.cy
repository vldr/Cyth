import "env"
    void log(string n)

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
c = log

log((string)b)
log((string)c)

# Object{field = 12}
# void(string)

log("" + b)
log("" + c)

# Object{field = 12}
# void(string)

log((string)log)
log((string)b.test)
log((string)test)
log((string)"".toArray)
log(test<string>())

# void(string)
# void(Object, int)
# string<T>()
# char[](string)
# string string

log("" + log)
log("" + b.test)
log("" + test)
log("" + "".toArray)

# void(string)
# void(Object, int)
# string<T>()
# char[](string)

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
