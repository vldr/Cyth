void print(int n)

int sqrt(int x)
    int s
    int b = 32768

    while b != 0
        int t = s + b 

        if t * t <= x
            s = t

        b = b / 2

    return s

int pow(int base, int exp)
    int result = 1

    while exp != 0
        if exp % 2 == 1
            result = result * base
            
        exp = exp / 2
        base = base * base
    
    return result

int result2 = sqrt(456420496)
int result = pow(result2, 2)

print(result2)
print(result)