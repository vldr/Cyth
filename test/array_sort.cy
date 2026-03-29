int[] array = [
    55, 47, 12, 47, 35, 15, 20, 42,
    30, 58, 15, 13, 19, 18, 44, 11, 
    7, 56, 17, 25, 14, 48, 4, 5, 7, 
    36, 1, 49, 25, 26, 30, 9
]

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
  
    while top
        h = stack.pop()
        l = stack.pop()

        top = top - 2
 
        int p = partition(l, h) 

        if p > 0 and p - 1 > l
            stack.push(l)
            stack.push(p - 1)

            top = top + 2
         
        if p + 1 < h
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