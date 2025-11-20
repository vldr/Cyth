import "env"
    void log(string n)
    void log(int n)

class Node
    string value
    Node[][] next
    Node[][] next2

    void __init__(string value)
        this.value = value

Node a = Node("a")
Node b = Node("b")
Node c = Node("c")

Node[] nodes
nodes.push(a)
nodes.push(b)
nodes.push(c)

a.next.push(nodes)
b.next.push(nodes)
c.next.push(nodes)

log(a.next[0][0].value)
log(b.next[0][1].value)
log(c.next[0][2].value)

log(a.value)
log(b.value)
log(c.value)

a.value = "a'"
b.value = "b'"
c.value = "c'"

log(a.next[0][0].value)
log(b.next[0][1].value)
log(c.next[0][2].value)

log(a.value)
log(b.value)
log(c.value)

a.next[0][0].value = "0"
b.next[0][1].value = "1"
c.next[0][2].value = "2"

log(a.next[0][0].value)
log(b.next[0][1].value)
log(c.next[0][2].value)

log(a.value)
log(b.value)
log(c.value)

nodes[0] = Node("a-")
nodes[1] = Node("b-")
nodes[2] = Node("c-")

log(a.next[0][0].value)
log(b.next[0][1].value)
log(c.next[0][2].value)

log(a.value)
log(b.value)
log(c.value)

# a
# b
# c
# a
# b
# c
# a'
# b'
# c'
# a'
# b'
# c'
# 0
# 1
# 2
# 0
# 1
# 2
# a-
# b-
# c-
# 0
# 1
# 2