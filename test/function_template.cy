import "env"
    void log(string n)
    void log(int n)

class Object
    int member = 22

    V caster<T, V>(T input)
        T innerFunc<T>()
            return member

        V casted = (V)input
        return casted + innerFunc<T>()

V caster<T, V>(T input)
    T innerFunc<T>()
        return outer

    V casted = (V)input
    return casted + innerFunc<T>()


int outer = 22

log(
    Object().caster<int, string>(10)
)

log(
    (string) caster<int, int>(11)
)

# 1022
# 33