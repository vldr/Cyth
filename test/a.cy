import "env"
    void log(int n)

void swap(int i, int j) 
    int temp = array[i]
    array[i] = array[j]
    array[j] = temp

int partition(int l, int h, bool(int, int) sorter) 
    int x = array[h]
    int i = l - 1
  
    for int j = l; j <= h - 1; j = j + 1
        if sorter(array[j], x)
            i += 1
            swap(i, j)
     
    swap(i + 1, h)
    return i + 1

void qsort(int l, int h, bool(int, int) sorter) 
    int[] stack
    stack.push(l)
    stack.push(h)

    int top = 2
  
    while (top > 0) 
     
        h = stack.pop()
        l = stack.pop()

        top = top - 2
 
        int p = partition(l, h, sorter) 

        if p > 0 and p - 1 > l
         
            stack.push(l)
            stack.push(p - 1)

            top = top + 2
         
  
        if (p + 1 < h) 
         
            stack.push(p + 1)
            stack.push(h)

            top = top + 2

bool sort(int a, int b)
  return a <= b

int[] array
array.reserve(100000)

for int i = 0; i < array.length; i += 1
  array[i] = array.length - i

qsort(0, array.length - 1, sort)