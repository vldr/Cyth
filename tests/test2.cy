import "env"
    void log(int n)

class Node
    int data
    Node next

    int getData()
        return data

    Node getNext()
        return next

    void setData(int data_)
        data = data_

    void setNext(Node next_)
        next = next_

Node a = Node()
Node b = Node()
Node c = Node()
Node d = Node()

a.setData(1)
a.setNext(b)

b.setData(2)
b.setNext(c)

c.setData(3)
c.setNext(d)

d.setData(4)

Node current = a
while current
    log(current.getData())
    current = current.getNext()

# 1
# 2
# 3
# 4