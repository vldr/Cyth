import "env"
    void log(string n)
    void log(int n)

class TreeNode
    TreeNode left
    TreeNode right
    
TreeNode bottomUpTree(TreeNode this, int depth)
    if depth > 0
        TreeNode leftChild = bottomUpTree(this, depth - 1)
        TreeNode rightChild = bottomUpTree(this, depth - 1)
        TreeNode node = TreeNode()
        node.left = leftChild
        node.right = rightChild
        return node
    else
        TreeNode node = TreeNode()
        node.left = null
        node.right = null
        return node

int itemCheck(TreeNode this) 
    if this.left == null
        return 1
    else
        return 1 + itemCheck(this.left) + itemCheck(this.right)

int minDepth = 4
int n = 20
int maxDepth
if minDepth + 2 > n
    maxDepth = minDepth + 2
else
    maxDepth = n

int stretchDepth = maxDepth + 1

TreeNode stretchTree = TreeNode()
TreeNode temp = bottomUpTree(stretchTree, stretchDepth)
int check = itemCheck(temp)
log("stretch tree of depth ")
log((string)stretchDepth)
log(" check: ")
log(check + "\n")

TreeNode longLivedTree = TreeNode()
longLivedTree = bottomUpTree(longLivedTree, maxDepth)

int depth = minDepth
while depth <= maxDepth
    int iterations = 1 << (maxDepth - depth + minDepth)
    check = 0
    
    int i = 1
    while i <= iterations
        TreeNode tree = TreeNode()
        tree = bottomUpTree(tree, depth)
        check = check + itemCheck(tree)
        i = i + 1
    
    log((string)iterations)
    log(" trees of depth ")
    log((string)depth)
    log(" check: ")
    log(check + "\n")
    depth = depth + 2

log("long lived tree of depth ")
log((string)maxDepth)
log(" check: ")
log(itemCheck(longLivedTree) + "\n")