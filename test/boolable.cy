import "env"
    void log(string n)
    void log(int n)

class Object

int a = -1
string b = "hello"
any c = Object()
Object d = Object()
void(string) e = log

if a
    log("integer")

if b
    log("string")

if c
    log("any")

if d
    log("class")

if e
    log("function pointer")

if null
    log("null")

# integer
# string
# any
# class
# function pointer

if true and a
    log("integer")

if false or a
    log("integer")

# integer
# integer

if true and b
    log("string")

if false or b
    log("string")

# string
# string

if true and c
    log("any")

if false or c
    log("any")

# any
# any

if true and d
    log("object")

if false or d
    log("object")

# object
# object

if true and e
    log("function pointer")

if false or e
    log("function pointer")

# function pointer
# function pointer

a = 0
b = ""
c = null
d = null
e = null

if not a
    log("integer")

if not b
    log("string")

if not c
    log("any")

if not d
    log("class")

if not e
    log("function pointer")

if not null
    log("null")

# integer
# string
# any
# class
# function pointer
# null

if true and not a
    log("integer")

if false or not a
    log("integer")

# integer
# integer

if true and not b
    log("string")

if false or not b
    log("string")

# string
# string

if true and not c
    log("any")

if false or not c
    log("any")

# any
# any

if true and not d
    log("object")

if false or not d
    log("object")

# object
# object

if true and not e
    log("function pointer")

if false or not e
    log("function pointer")

# function pointer
# function pointer
