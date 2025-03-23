import "env"
    void log(int n)

class Node
    int value
    Node next

    void __init__(int value, Node next)
        this.value = value
        this.next = next

class PriorityQueue
    int size
    Node front
    Node back

    void enqueue(int value)
        if front != null
            Node previous
            Node current = front

            while current and current.value < value
                previous = current
                current = current.next

            if previous and current
                previous.next = Node(value, previous.next)
            else if previous
                back.next = Node(value, null)
                back = back.next
            else
                front = Node(value, front)

        else
            front = Node(value, null)
            back = front

        size = size + 1

    Node dequeue()
        Node node = null

        if front
            node = front

            if front == back
                front = back = null
            else
                front = front.next

            size = size - 1

        return node

    Node peek()
        return front

    bool empty()
        return size == 0

int items = 10
PriorityQueue pq = PriorityQueue()

for int i = items; i >= 0; i = i - 1
    pq.enqueue(i)

    log(pq.peek().value)

for int i = 0; i <= items; i += 1
    log(pq.dequeue().value)

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