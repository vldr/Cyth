import "env"
    void log(string n)

class Apple
    int x = 2

    void test()
        log("" + x)

    void test(int f)
        log("" + x+f)

Apple.test(Apple())
Apple().test()
Apple().test(10)

# 2
# 2
# 210

class Banana
    int x = 5

    void test()
        log("" + x)

Banana.test(Banana())
Banana().test()

# 5
# 5