import "env"
  void log(int n)

class QuickSort<T>
  bool(QuickSort<T>, T, T) comparator = comp

  void __init__(bool(QuickSort<T>, T, T) comparator)
    this.comparator = comparator

  bool comp(T a, T b)
    return a < b

  void sort(T[] array)
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

  int partition(T[] array, int low, int high)
    T pivot = array[high]
    int i = low - 1

    for int j = low; j < high; j = j + 1
      if comparator(this, array[j], pivot)
        i = i + 1

        swap(array, i, j)

    swap(array, i + 1, high)
    return i + 1

  void swap(T[] array, int i, int j)
    T temp = array[i]
    array[i] = array[j]
    array[j] = temp

int[] array = [
  55, 47, 35, 15, 20, 42,
  52, 30, 58, 15, 13, 19,
  32, 18, 44, 11, 7, 9,
  34, 56, 17, 25, 14, 48,
  40, 4, 5, 7, 36, 1,
  33, 49, 25, 26, 30, 9
]

bool int_comparator(QuickSort<int> self, int a, int b)
    return a > b

QuickSort<int> sorter = QuickSort<int>(int_comparator)
sorter.sort(array)

for int i = 0; i < array.length; i = i + 1
  log(array[i])

# 58
# 56
# 55
# 52
# 49
# 48
# 47
# 44
# 42
# 40
# 36
# 35
# 34
# 33
# 32
# 30
# 30
# 26
# 25
# 25
# 20
# 19
# 18
# 17
# 15
# 15
# 14
# 13
# 11
# 9
# 9
# 7
# 7
# 5
# 4
# 1