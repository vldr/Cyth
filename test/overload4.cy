import "env"
    void log(string n)

void test(int a, int b)
    log(a + " " + b)

void test(float a, float b)
    log(a + " " + b)

void(int, int) a = (void(int,int))test
void(float, float) b = (void(float,float))test

a(10, 10)
b(12.5, 12.5)

# 10 10
# 12.5 12.5