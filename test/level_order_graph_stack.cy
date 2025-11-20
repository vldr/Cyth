import "env"
    void log(string n)
    void log(int n)

class Tree
    Tree left
    Tree right
    string value

    void __init__(string value)
        this.value = value

void print(Tree[] list)
    for int i = 0; i < list.length; i += 1
        log(list[i].value)

void traverse(Tree[] list)
    int level

    while list.length
        log("_________ Level: " + level + " _________")
        print(list)
        
        Tree[] temp

        while list.length > 0
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

Tree[] stack
stack.push(a)
traverse(stack)

# _________ Level: 0 _________
# 1
# _________ Level: 1 _________
# 2
# 3
# _________ Level: 2 _________
# 5
# 6
# 4
# _________ Level: 3 _________
# 7
# 8