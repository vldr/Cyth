import "env"
  void log(int n)

class QuickSort<T>
  void sort(T[] array)
    int test = 10
    void swap(T[] array, int i, int j)
        T temp = array[i]
        array[i] = array[j]
        array[j] = temp

    int partition(T[] array, int low, int high)
        T pivot = array[high]
        int i = low - 1

        for int j = low; j < high; j = j + 1
            if array[j] <= pivot
                i = i + 1

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
  

int[] array = [
  55, 47, 35, 15, 20, 42,
  52, 30, 58, 15, 13, 19,
  32, 18, 44, 11, 7, 9,
  34, 56, 17, 25, 14, 48,
  40, 4, 5, 7, 36, 1,
  33, 49, 25, 26, 30, 9
]

QuickSort<int> sorter = QuickSort<int>()
sorter.sort(array)

for int i = 0; i < array.length; i = i + 1
  log(array[i])

# 1
# 4
# 5
# 7
# 7
# 9
# 9
# 11
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
# 32
# 33
# 34
# 35
# 36
# 40
# 42
# 44
# 47
# 48
# 49
# 52
# 55
# 56
# 58