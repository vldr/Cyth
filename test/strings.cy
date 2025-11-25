import "env"
    void log(bool n)
    void log(int n)

string a = "hello"
string b = "world"
string c = a + b

log(a == a)
log(b == b)
log(a == b)
log(c == a + b)

log(a != a)
log(b != b)
log(a != b)
log(c != a + b)

# 1
# 1
# 0
# 1

# 0
# 0
# 1
# 0