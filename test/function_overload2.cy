import "env"
    void log(int n)
    void log(string n)

void __init__()
    log("__init__")

void __init__(int value)
    log("__init__ " + value)

void __init__<T>()
    log("__init__<T>")

void __init__<T>(T value)
    log("__init__<T> " + value)

void() b = (void()) __init__
b()
# __init__

b = (void()) __init__<int>
b()
# __init__<T>

void(int) c = (void(int)) __init__
c(12)
# __init__ 12

c = (void(int)) __init__<int>
c(12)
# __init__<T> 12


