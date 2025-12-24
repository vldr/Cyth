import "env"
    void log(int n)
    void log(string n)

void __init__()
    log("__init__")

void() a = __init__
a()
# __init__

a = (void())__init__
a()
# __init__