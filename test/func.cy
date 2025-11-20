import "env"
    void log(string n)
    void log(int n)

int functor(int a, int b)
    log("functor " + a + " " + b)
    return a+b

int run_my_func(int(int, int) func)
    return func(10, 2)

run_my_func(functor)

# functor 10 2