import "env"
    void log(string n)

class Test
    int[][][] a

    void __init__(int[][][] a)
        this.a = a

int[][][] b = [ [[]], [[],[]]]
int[][][] c = [[[], [], [8]], [[], [], []]]

b = [[[]]]

int[][][] getC()
    return c

int[][][] getB()
    return b

int[][][] getF(int[][][] a)
    a.push( [[ ]] )
    return a

getF([ [[]], [[],[]]])

Test test()
    return Test([ [[]], [[],[]]])
