class Object

int a = -1
string b = "hello"
any c = Object()
Object d = Object()
void(string) e = (void(string))log
int[] f = [1,2,3]

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

if f
    log("array")


if true and a
    log("integer")

if false or a
    log("integer")


if true and b
    log("string")

if false or b
    log("string")


if true and c
    log("any")

if false or c
    log("any")


if true and d
    log("object")

if false or d
    log("object")


if true and e
    log("function pointer")

if false or e
    log("function pointer")


if true and f
    log("array")

if false or f
    log("array")


a = 0
b = ""
c = null
d = null
e = null
f = []

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

if not f
    log("array")

if not null
    log("null")


if true and not a
    log("integer")

if false or not a
    log("integer")


if true and not b
    log("string")

if false or not b
    log("string")


if true and not c
    log("any")

if false or not c
    log("any")


if true and not d
    log("object")

if false or not d
    log("object")


if true and not e
    log("function pointer")

if false or not e
    log("function pointer")


if true and not f
    log("array")

if false or not f
    log("array")

#! 16:4-16:5 Cannot access 'c' because it is inaccessible.
#! 46:13-46:14 Cannot access 'c' because it is inaccessible.
#! 49:13-49:14 Cannot access 'c' because it is inaccessible.
#! 76:1-76:2 Cannot access 'c' because it is inaccessible.
#! 87:8-87:9 Cannot access 'c' because it is inaccessible.
#! 117:17-117:18 Cannot access 'c' because it is inaccessible.
#! 120:17-120:18 Cannot access 'c' because it is inaccessible.