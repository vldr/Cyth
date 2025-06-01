import "env"
    void log(int n)

int modpow(int a, int b, int n) 
    int x = 1
    int y = a

    while b
        if b % 2 == 1
            x = (x * y) % n
        
        y = (y * y) % n
        b = b / 2
    
    return x % n

class Solution
    int root1
    int root2
    bool exists

    void __init__(int root1, int root2, bool exists)
        this.root1 = root1
        this.root2 = root2
        this.exists = exists

Solution makeSolution(int root1, int root2, bool exists) 
    Solution sol = Solution(root1, root2, exists)
    return sol

Solution ts(int n, int p) 
    int q = p - 1
    int ss = 0
    int z = 2
    int c
    int r
    int t
    int m

    if modpow(n, (p - 1) / 2, p) != 1
        return makeSolution(0, 0, false)
    
    while not (q % 2)
        ss = ss + 1
        q = q / 2

    if ss == 1 
        int r1 = modpow(n, (p + 1) / 4, p)
        return makeSolution(r1, p - r1, true)
    
    while (modpow(z, (p - 1) / 2, p) != p - 1) 
        z = z + 1

    c = modpow(z, q, p)
    r = modpow(n, (q + 1) / 2, p)
    t = modpow(n, q, p)
    m = ss

    while true
        int i = 0
        int zz = t
        int b = c
        int e

        if t == 1
            return makeSolution(r, p - r, true)
        
        while zz != 1 and i < (m - 1) 
            zz = zz * zz % p
            i += 1
        
        e = m - i - 1
        while e > 0 
            b = b * b % p
            e = e - 1
        
        r = r * b % p
        c = b * b % p
        t = t * c % p
        m = i
        
    return null

void test(int n, int p)
    Solution sol = ts(n, p)
    if sol.exists
        log(sol.root1)
        log(sol.root2)

test(10, 13)
test(56, 101)
test(1030, 10009)
test(1032, 10009)
test(44402, 100049)

# 7
# 6
# 37
# 64
# 1632
# 8377