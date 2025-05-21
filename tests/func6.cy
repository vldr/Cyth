import "env"
  void log(bool n)

class QuickSort<T>
  bool(QuickSort<T>, T, T) comparator

  bool comp(T a, T b)
    return a < b

  bool run(int a, int b)
    return comparator(this, a, b)

QuickSort<int> sorter = QuickSort<int>()
sorter.comparator = sorter.comp

log(
  sorter.run(10, 20)
)

bool comp(QuickSort<int> this, int a, int b)
  return a > b

sorter.comparator = comp

log(
  sorter.run(10, 20)
)

# 1
# 0
