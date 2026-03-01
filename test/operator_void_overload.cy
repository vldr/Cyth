class Test
    int cool

    void __add__(Test other)
        log("__add__")
    
    void __sub__(Test other)
        log("__sub__")
    
    void __div__(Test other)
        log("__div__")
    
    void __mul__(Test other)
        log("__mul__")

    void __eq__(Test other)
        log("__eq__")

    void __ne__(Test other)
        log("__ne__")

    void __lt__(Test other)
        log("__lt__")

    void __le__(Test other)
        log("__le__")
        
    void __ge__(Test other)
        log("__ge__")

    void __gt__(Test other)
        log("__gt__")

    void __mod__(Test other)
        log("__mod__")

    void __and__(Test other)
        log("__and__")

    void __or__(Test other)
        log("__or__")

    void __xor__(Test other)
        log("__xor__")

    void __lshift__(Test other)
        log("__lshift__")

    void __rshift__(Test other)
        log("__rshift__")  

    void __set__(string index, string value)
        log("Set Index = " + index + " Value = " + value)

    void __get__(string index)
        log("Get Index = " + index)

Test test = Test()
test["hello"]
test["world"] = "bar"

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

# Get Index = hello
# Set Index = world Value = bar
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