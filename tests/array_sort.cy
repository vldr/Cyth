import "env"
    void log(int n) 

int[] array
array.push(55)
array.push(47)
array.push(12)
array.push(47)
array.push(35)
array.push(15)
array.push(20)
array.push(42)
array.push(30)
array.push(58)
array.push(15)
array.push(13)
array.push(19)
array.push(18)
array.push(44)
array.push(11)
array.push(7)
array.push(56)
array.push(17)
array.push(25)
array.push(14)
array.push(48)
array.push(4)
array.push(5)
array.push(7)
array.push(36)
array.push(1)
array.push(49)
array.push(25)
array.push(26)
array.push(30)
array.push(9)

void swap(int i, int j) 
    int temp = array[i]
    array[i] = array[j]
    array[j] = temp

int partition(int l, int h) 
    int x = array[h]
    int i = l - 1
  
    for int j = l; j <= h - 1; j = j + 1
        if array[j] <= x
            i += 1
            swap(i, j)
         
     
    swap(i + 1, h)

    return i + 1
 

void qsort(int l, int h) 
    int[] stack
    stack.push(l)
    stack.push(h)

    int top = 2
  
    while (top > 0) 
     
        h = stack.pop()
        l = stack.pop()

        top = top - 2
 
        int p = partition(l, h) 

        if p > 0 and p - 1 > l
         
            stack.push(l)
            stack.push(p - 1)

            top = top + 2
         
  
        if (p + 1 < h) 
         
            stack.push(p + 1)
            stack.push(h)

            top = top + 2

qsort(0, array.length - 1)

for int i = 0; i < array.length; i += 1
    log(array[i])

# 1
# 4
# 5
# 7
# 7
# 9
# 11
# 12
# 13
# 14
# 15
# 15
# 17
# 18
# 19
# 20
# 25
# 25
# 26
# 30
# 30
# 35
# 36
# 42
# 44
# 47
# 47
# 48
# 49
# 55
# 56
# 58