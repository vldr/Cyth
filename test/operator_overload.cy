import "env"
    void log(string n)
    void log(int n)

class Test
    int cool

    Test __add__(Test other)
        log("__add__")
        return this
    
    Test __sub__(Test other)
        log("__sub__")
        return this
    
    Test __div__(Test other)
        log("__div__")
        return this
    
    Test __mul__(Test other)
        log("__mul__")
        return this

    Test __eq__(Test other)
        log("__eq__")
        return this

    Test __ne__(Test other)
        log("__ne__")
        return this

    Test __lt__(Test other)
        log("__lt__")
        return this

    Test __le__(Test other)
        log("__le__")
        return this
        
    Test __ge__(Test other)
        log("__ge__")
        return this

    Test __gt__(Test other)
        log("__gt__")
        return this

    Test __mod__(Test other)
        log("__mod__")
        return this

    Test __and__(Test other)
        log("__and__")
        return this

    Test __or__(Test other)
        log("__or__")
        return this

    Test __xor__(Test other)
        log("__xor__")
        return this

    Test __lshift__(Test other)
        log("__lshift__")
        return this

    Test __rshift__(Test other)
        log("__rshift__")
        return this  

    string __set__(string index, string value)
        log("Set Index = " + index + " Value = " + value)
        return value

    string __get__(string index)
        log("Get Index = " + index)
        return "foo"

Test test = Test()
log(test["hello"])
log(test["world"] = "bar")

Test() + Test()
Test() - Test()
Test() / Test()
Test() * Test()
Test() == Test()
Test() != Test()
Test() < Test()
Test() <= Test()
Test() > Test()
Test() % Test()
Test() & Test()
Test() | Test()
Test() ^ Test()
Test() << Test()
Test() >> Test()

class Test2

Test2 a = Test2()
Test2 b = Test2()

log((string)(a == a))
log((string)(a != a))
log((string)(a == b))
log((string)(a != b))

# Get Index = hello
# foo
# Set Index = world Value = bar
# bar
# __add__
# __sub__
# __div__
# __mul__
# __eq__
# __ne__
# __lt__
# __le__
# __gt__
# __mod__
# __and__
# __or__
# __xor__
# __lshift__
# __rshift__
# true
# false
# false
# true