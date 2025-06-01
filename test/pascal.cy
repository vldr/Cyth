import "env"
    void log(int n)

int binomialCoeff(int n, int k) 
    int res = 1

    if k > n - k 
        k = n - k

    int i
    while i < k
        res = res * (n - i)
        res = res / (i + 1)

        i += 1
      
    return res

int index
int count = 16

while index < count
    log(
        binomialCoeff(count  - 1, index)
    )

    index = index + 1

# 1
# 15
# 105
# 455
# 1365
# 3003
# 5005
# 6435
# 6435
# 5005
# 3003
# 1365
# 455
# 105
# 15
# 1