import "env"
    void log(float n)

class Node
    int value = 12

class A<T>
    T b
    
    void __init__(T value)
        b = value

    T set(T value)
        b = value
        return b

    T get()
        return b

log(
    A<float>(12.2).set(14.0)
)

# 14