import "env"
    void log(int n)
    void log(string n)

class A
    int field = 10

    void __init__()
        log("__init__")

void(A) a = A.__init__
int b = A.field

#! 12:11-12:16 No member named 'field' in 'class A'.