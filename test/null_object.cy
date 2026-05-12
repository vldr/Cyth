class Node
    int value
    Node next

    void __init__(int value, Node next)
        this.value = value
        this.next = next

Node a = Node(10, null)
log(a.value)

# 10

log(a.next.value)

#> Invalid memory or null pointer access
#>   at <start>:14:12

#< RuntimeError: dereferencing a null pointer