import "env"
    void log(string n)

void test(int a, int b)
    log(a + " " + b)

void test(float a, float b)
    log(a + " " + b)

void(int, int) a = (void(int,float))test

#! 10:21-10:25 Invalid type conversion from 'void(int, int), void(float, float)' to 'void(int, float)'.