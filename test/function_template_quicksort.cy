import "env"
    void log(string n)
    void log(int n)

int[] a = [3, 5, 8, 1, 20, 1]
float[] b = [3.0, 5.0, 8.0, 1.0, 20.0, 1.0]

quickSort<int>(a)
quickSort<float>(b)

for int i = 0; i < a.length; i += 1
    log((string)a[i])

log("-------------------------")

for int i = 0; i < b.length; i += 1
    log((string)b[i])

# 1
# 1
# 3
# 5
# 8
# 20
# -------------------------
# 1
# 1
# 3
# 5
# 8
# 20

void quickSort<T>(T[] array)
    void swap(T[] array, int i, int j)
        T temp = array[i]
        array[i] = array[j]
        array[j] = temp
    
    int partition(T[] array, int low, int high)
        T pivot = array[high]
        int i = low - 1

        for int j = low; j < high; j = j + 1
            if array[j] <= pivot
                i += 1
                
                swap(array, i, j)

        swap(array, i + 1, high)
        return i + 1


    int low = 0
    int high = array.length - 1
    
    if low >= high
        return
    
    int[] stack 
    stack.push(low)
    stack.push(high)

    while stack.length > 0
        high = stack.pop()
        low = stack.pop()

        int p = partition(array, low, high)

        if p - 1 > low
            stack.push(low)
            stack.push(p - 1)

        if p + 1 < high
            stack.push(p + 1)
            stack.push(high)


