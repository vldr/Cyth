import "env"
    void log(string n)
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
        
        if not head
            head = node
        else
            Node current = head
            while current.next
                current = current.next

            current.next = node

        size = size + 1

    Node get(int index)
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

int split(LinkedList nums)
        int n = nums.size
        int splits = 0
        int leftSum = 0
        int rightSum = 0

        for int i = 0; i < n; i += 1
            rightSum = rightSum + nums.get(i).data
        
        for int i = 0; i < n - 1; i += 1
            leftSum = leftSum + nums.get(i).data
            rightSum = rightSum - nums.get(i).data

            if leftSum >= rightSum
                splits = splits + 1

        return splits

LinkedList list = LinkedList()
list.append(10)
list.append(4)
list.append(-8)
list.append(7)

log("Splits: " + split(list))

list = LinkedList()
list.append(2)
list.append(3)
list.append(1)
list.append(0)

log("Splits: " + split(list))

# Splits: 2
# Splits: 2