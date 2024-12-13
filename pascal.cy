void log(int n)

int binomialCoeff(int n, int k) 
    int res = 1

    if k > n - k 
        k = n - k

    int i
    while i < k
        res = res * (n - i)
        res = res / (i + 1)

        i = i + 1
      
    return res

int index
int count = 16

while index < count
    log(
        binomialCoeff(count  - 1, index)
    )

    index = index + 1