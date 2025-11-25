import "env"
  void log(string n)
  void log(int n)
  
class Node
  int value
  Node next
  Node prev

  void __init__(int value, Node next, Node prev)
    this.value = value
    this.next = next
    this.prev = prev

Node a = Node(10, null, null)
Node b = Node(12, null, null)
Node c = Node(13, null, null)
Node d = Node(14, null, null)

a.prev = d
a.next = b
b.prev = a

b.prev = a
b.next = c

c.next = d
c.prev = b

d.prev = c
d.next = a

log((string)a)

# Node(\n value = 10,\n next = Node$1(\n  value = 12,\n  next = Node$2(\n   value = 13,\n   next = Node$3(\n    value = 14,\n    next = Node(...),\n    prev = Node$2(...)\n   ),\n   prev = Node$1(...)\n  ),\n  prev = Node(...)\n ),\n prev = Node$3(...)\n)