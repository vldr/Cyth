import "env"
    void log(int n)
    void log(string n)

class A
    void __init__()
        log("A.__init__")

void(A) a = A.__init__
a(null)
# A.__init__

a = (void(A))A.__init__
a(null)
# A.__init__

class B
    void __init__()
        log("B.__init__")

    void __init__(int value)
        log("B.__init__ " + value)

    void __init__<T>()
        log("B.__init__<T>")

    void __init__<T>(T value)
        log("B.__init__<T> " + value)

void(B) b = (void(B)) B.__init__
b(null)
# B.__init__

b = (void(B)) B.__init__<int>
b(null)
# B.__init__<T>

void(B, int) c = (void(B, int)) B.__init__
c(null, 12)
# B.__init__ 12

c = (void(B, int)) B.__init__<int>
c(null, 12)
# B.__init__<T> 12


