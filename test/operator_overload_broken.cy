





class A
    int a

    void __get__(int a)
        log(a)

    Thing __set__(int a, Thing b)
        log(a + " " + (string)b)

        return b
    
    bool __eq__()
        return false

log(A() == A())

any Thing = string

#! 18:10-18:16 The '__eq__' method must have one argument.
#! 21:9-21:11 Operator '==' is not defined for 'A' and 'A' (missing __eq__ method).
