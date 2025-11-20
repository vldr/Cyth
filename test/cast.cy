import "env"
    void log(string n)
    void log(int n)

int test()
    return 10

class Test
    int data = 9

float a = (float)10
bool b = (bool)10
bool c = (bool)0

int d = (int)12.5
bool e = (bool)12.5
bool f = (bool)0.0

int g = (int)true
float h = (float)true

int i = (int)false
float j = (float)false

float k = (float)Test().data
float l = (float)test()

bool m = (bool)test()
bool n = (bool)Test().data

string o = (string)"test"

log("" + a)
log("" + b)
log("" + c)
log("" + d)
log("" + e)
log("" + f)
log("" + g)
log("" + h)
log("" + i)
log("" + j)
log("" + k)
log("" + l)
log("" + m)
log("" + n)
log("" + o)

# 10
# true
# false

# 12
# true
# false

# 1
# 1

# 0
# 0

# 9
# 10

# true
# true
# test