void log(int n)

class Tree
    Tree left
    Tree right
    int value

    void __init__(int value)
        this.value = value

class Node
    Tree data
    Node next

class Stack
    Node head
    int size

    Tree pop()
        return del(0)

    void push(Tree data)
        Node node = Node()
        node.data = data

        if not head
            head = node
        else
            Node current = head
            while current.next 
                current = current.next

            current.next = node

        size = size + 1

    Tree del(int index)
        Tree found
        int currentIndex
        Node previous
        Node current = head

        while current 
            if index == currentIndex
                found = current.data
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
        for Node node = head; node ; node = node.next
            log(node.data.value)

void traverse(Stack list)
    while list.size
        list.print()
        log(-1)
        
        Stack temp = Stack()

        while list.size > 0
            Tree node = list.pop()

            if node.left 
                temp.push(node.left)
                
            if node.right 
                temp.push(node.right)
                
        list = temp

Tree a = Tree(1)
Tree b = Tree(2)
Tree c = Tree(3)
Tree d = Tree(4)
Tree e = Tree(5)
Tree f = Tree(6)
Tree g = Tree(7)
Tree h = Tree(8)

a.left = b
a.right = c
b.left = d
c.left = e
c.right = f

d.left = g
f.right = h

Stack stack = Stack()
stack.push(a)
traverse(stack)

# 1
# -1
# 2
# 3
# -1
# 4
# 5
# 6
# -1
# 7
# 8
# -1