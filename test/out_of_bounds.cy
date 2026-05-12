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
            swap(i, j+1000)
         
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

#> Invalid memory or null pointer access
#>   at swap.void(int, int):10:22
#>   at partition.int(int, int):20:13
#>   at qsort.void(int, int):39:17
#>   at <start>:53:1

#< RuntimeError: array element access out of bounds