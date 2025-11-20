import "env"
    void log(string n)
    void log(int n)

class Tree
    Tree left
    Tree right
    string value

    void __init__(string value)
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
    int level

    while list.size
        log("_________ Level: " + level + " _________")
        list.print()
        
        Stack temp = Stack()

        while list.size > 0
            Tree node = list.pop()

            if node.left 
                temp.push(node.left)
                
            if node.right 
                temp.push(node.right)
                
        list = temp
        level = level + 1

Tree a = Tree("1")
Tree b = Tree("2")
Tree c = Tree("3")
Tree d = Tree("4")
Tree e = Tree("5")
Tree f = Tree("6")
Tree g = Tree("7")
Tree h = Tree("8")

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

# _________ Level: 0 _________
# 1
# _________ Level: 1 _________
# 2
# 3
# _________ Level: 2 _________
# 4
# 5
# 6
# _________ Level: 3 _________
# 7
# 8