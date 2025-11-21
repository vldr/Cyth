import "env"
    void log(string n)
    void log(int n)

class Test
    Test2 test2
    int data

    void __init__(int data)
        this.data = data

class Test2
    Test test
    float data

    void __init__(float data)
        this.data = data


Test2 a = Test2(12.0)
Test b = Test(10)
Test2 c = Test2(3.5)

a.test = b
b.test2 = c

log((string) a.data)
log((string) a.test.data)
log((string) a.test.test2.data)

# 12
# 10
# 3.5