import "env"
    void log(int n)

class Node
    int data
    Node next

    void __init__(int data)
        this.data = data

    bool __eq__(Node other)
        return this.data == other.data

class LinkedList
    Node head
    int size

    void append(int data)
        Node node = Node(data)

        if not head
            head = node
        else
            Node current = head
            while current.next
                current = current.next

            current.next = node

        size = size + 1

    void prepend(int data)
        Node node = Node(data)

        if not head
            head = node
        else
            node.next = head
            head = node

        size = size + 1

    bool pop()
        return del(size - 1)

    bool popfront()
        return del(0)

    Node at(int index)
        Node found
        int currentIndex
        Node current = head

        while current
            if index == currentIndex
                found = current
                break

            current = current.next
            currentIndex = currentIndex + 1
            
        return found

    void sort()
        int max

        for int i = size; i >= 1; i = i - 1
            max = 0

            for int j = 0; j < i; j = j + 1
                if at(j).data > at(max).data 
                    max = j
            
            int temp = at(i - 1).data
            at(i - 1).data = at(max).data
            at(max).data = temp

    bool del(int index)
        bool found
        int currentIndex
        Node previous
        Node current = head

        while current
            if index == currentIndex
                found = true
                break

            previous = current
            current = current.next
            currentIndex = currentIndex + 1

        if found
            if not previous
                head = current.next
            else
                previous.next = current.next

            size = size - 1
            
        return found

    void print()
        for Node node = head; node; node = node.next
            log(node.data)
            
LinkedList list = LinkedList()

int items = 10
for int i = items; i >= -items; i = i - 1
    list.append(i)

list.print()
list.sort()
list.print()

# 10
# 9
# 8
# 7
# 6
# 5
# 4
# 3
# 2
# 1
# 0
# -1
# -2
# -3
# -4
# -5
# -6
# -7
# -8
# -9
# -10
# -10
# -9
# -8
# -7
# -6
# -5
# -4
# -3
# -2
# -1
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