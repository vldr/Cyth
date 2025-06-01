import "env"
    void log(int n)

class A
    int outer = 10
  
    void hello(int n)
        log(outer)


        void test()
            log(n)

        int b
        void test2()
            log(b)


int outer = 10
  
void hello(int n)
    log(outer)

    void test()
        log(n)

for int i = 0; i < 2; i += 1
    void test()
        log(outer)
        log(i)

#! 12:17-12:18 Cannot access 'n' because it is outside of the function.
#! 16:17-16:18 Cannot access 'b' because it is outside of the function.
#! 25:13-25:14 Cannot access 'n' because it is outside of the function.
#! 30:13-30:14 Cannot access 'i' because it is outside of the function.