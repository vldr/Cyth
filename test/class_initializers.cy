import "env"
    void log(int n)

int helloworld = 22

class A
    int data = helloworld

class B
    int data = test() + 1
    A a = test2()

    void __init__(int data)
        this.data = this.data + data

    int test()
        return this.data + 10

    A test2()
        return A()

B a = B(12)
log(a.data)
log(a.a.data)

# 23
# 22