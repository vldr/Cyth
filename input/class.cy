void log(int n)

class Node
    int data
    Node next

class LinkedList
    Node head

    void push(int data)
        Node node = Node()
        node.data = data

        if head == null
            head = node
        else
            Node current = head
            while current.next != null
                current = current.next

            current.next = node

    void del(int index)
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

        if not found
            return

        if previous == null
            head = current.next
        else
            previous.next = current.next

    void print()
        for Node node = head; node != null; node = node.next
            log(node.data)
        

int items = 100
LinkedList list = LinkedList()

for int i = 0; i < items; i = i + 1
    list.push(i)

for int i = 0; i < items; i = i + 1
    list.del(0)



list.print()