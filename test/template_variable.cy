
import "env"
    void log(int hello)

class Node<T>
    T node

    void __init__()
        log(10)

Node<Node<int>> hello = Node<Node<int>>()
hello.node = Node<int>()

class Test<K, V>
    K node
    V pode

    void __init__()
        log(node)

Test<int, Node<float>>()
Test<int, Node<Node<float>>>()

# 10
# 10
# 0
# 0