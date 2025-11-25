import "env"
    void log(float n)
    void log(int n)

int sqrt(int x)
    int s
    int b = 32768

    while b
        int t = s + b 

        if t * t <= x
            s = t

        b = b / 2

    return s

int pow(int base, int exp)
    int result = 1

    while exp
        if exp % 2 == 1
            result = result * base
            
        exp = exp / 2
        base = base * base
    
    return result

int result2 = sqrt(456420496)
int result = pow(result2, 2)

log(result2 + 0.0)
log(result + 0.0)

float sqrtf(float n)
    float x = n
    float y = 1.0
    float e = 0.000001

    while x - y > e 
        x = (x + y) / 2.0
        y = n / x

    return x

log(sqrtf(64.0))

float exponential(float n, float x)
    float sum = 1.0
  
    for float i = n - 1; i > 0; i = i - 1
        sum = (x * sum / i) + 1.0  
  
    return sum  
 
float n = 7.0
float x = 0.0

log(exponential(n, x))

# 21364
# 456420480
# 8
# 1