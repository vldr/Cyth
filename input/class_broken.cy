void log(int n)

class Node
    int data
    Node next

    void __init__(int data)
        this.data = data

class LinkedList
    Node head
    int size

    void append(int data)
        Node node = Node(data)

        if head == null
            head = node
        else
            Node current = head
            while current.next != null
                current = current.next

            current.next = node

        size = size + 1

    void prepend(int data)
        Node node = Node(data)

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
LinkedList list = LinkedList()

for int i = 0; i < items; i = i + 1
    list.prepend(i)

for i = 0; i < items / 2; i = i + 1
    list.pop()
    
list.print()

# 99
# 98
# 97
# 96
# 95
# 94
# 93
# 92
# 91
# 90
# 89
# 88
# 87
# 86
# 85
# 84
# 83
# 82
# 81
# 80
# 79
# 78
# 77
# 76
# 75
# 74
# 73
# 72
# 71
# 70
# 69
# 68
# 67
# 66
# 65
# 64
# 63
# 62
# 61
# 60
# 59
# 58
# 57
# 56
# 55
# 54
# 53
# 52
# 51
# 50