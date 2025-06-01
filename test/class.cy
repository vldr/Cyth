import "env"
    void log(int n)

class Node
    int data
    Node next

class LinkedList
    Node head
    int size

    void __init__(int size, Node head)
        this.size = size
        this.head = head

    void append(int data)
        Node node = Node()
        node.data = data

        if head == null
            head = node
        else
            Node current = head
            while current.next != null
                current = current.next

            current.next = node

        size = size + 1

    void prepend(int data)
        Node node = Node()
        node.data = data

        if head == null
            head = node
        else
            node.next = head
            head = node

        size = size + 1

    bool pop()
        return del(size - 1)

    bool popfront()
        return del(0)

    bool del(int index)
        bool found
        int currentIndex
        Node previous
        Node current = head

        while current != null
            if index == currentIndex
                found = true
                break

            previous = current
            current = current.next
            currentIndex = currentIndex + 1

        if found
            if previous == null
                head = current.next
            else
                previous.next = current.next

            size = size - 1
            
        return found

    void print()
        for Node node = head; node != null; node = node.next
            log(node.data)
            
int items = 100
Node node = Node()
LinkedList list = LinkedList(0, null)

for int i = 0; i < items; i += 1
    list.append(i)

for int i = 0; i < items / 2; i += 1
    list.pop()
    
list.print()

# 0
# 1
# 2
# 3
# 4
# 5
# 6
# 7
# 8
# 9
# 10
# 11
# 12
# 13
# 14
# 15
# 16
# 17
# 18
# 19
# 20
# 21
# 22
# 23
# 24
# 25
# 26
# 27
# 28
# 29
# 30
# 31
# 32
# 33
# 34
# 35
# 36
# 37
# 38
# 39
# 40
# 41
# 42
# 43
# 44
# 45
# 46
# 47
# 48
# 49