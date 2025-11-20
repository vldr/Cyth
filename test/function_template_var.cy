import "env"
    void log(string n)
    void log(int n)

T functor<T>(T a, T b)
    log("functor " + a + " " + b)
    return a+b

T run_my_func<T>(T(T, T) func)
    return func(10, 2)

run_my_func<int>(functor<int>)

class A
    T functor<T>(T a, T b)
        log("functor " + a + " " + b)
        return a+b

    T run_my_func<T>(T(A, T, T) func)
        return func(this, 10, 2)

A().run_my_func<int>(A().functor<int>)
A().functor<int>
A().functor<int>
A().run_my_func<int>
A().run_my_func<int>

# functor 10 2
# functor 10 2