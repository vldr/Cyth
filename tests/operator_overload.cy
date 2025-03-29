import "env"
    void log(string n)

class Test
    int cool

    void __set__(string index, string value)
        log("Set Index = " + index + " Value = " + value)

    string __get__(string index)
        log("Get Index = " + index)
        return "foo"

Test test = Test()
log(test["hello"])
log(test["world"] = "bar")

# Get Index = hello
# foo
# Set Index = world Value = bar
# bar