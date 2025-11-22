import "env"
    void log(string n)
    void log(int n)

void test<T>(T a, T b)
    log(a + " " + b)

void test(int a, int b)
    log(a + " " + b)

void test(float a, float b)
    log(a + " " + b)

void(int, int) a = (void(int,int))test
void(int, int) b = (void(int,int))test<int>
void(float, float) c = (void(float,float))test

a(10, 10)
b(11, 11)
c(12.5, 12.5)

# 10 10
# 11 11
# 12.5 12.5