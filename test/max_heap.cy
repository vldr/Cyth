import "env"
  void log(string n)
  void log(int n)

class MaxHeap<T>
  T[] data
  int size

  void insert(T value)
    if size == data.length
      data.push(value)
    else
      data[size] = value

    size += 1

    int i = size - 1

    while i
      T parent = data[(i - 1) / 2]
      T child = data[i]

      if parent < child
        T temp = parent
        data[(i - 1) / 2] = child
        data[i] = temp

        i = (i - 1) / 2
      else
        break

  T peek()
    return data[0]

  T pop()
    int i = 0

    T value = data[i]
    data[i] = data[size - 1]
    size -= 1

    while true
      int max = i
      int left_child = 2 * i + 1
      int right_child = 2 * i + 2

      if left_child < size and data[left_child] > data[max]
        max = left_child

      if right_child < size and data[right_child] > data[max]
        max = right_child

      if max != i
        T temp = data[i]
        data[i] = data[max]
        data[max] = temp

        i = max
      else
        break

    return value

MaxHeap<int> a = MaxHeap<int>()
a.insert(10)
a.insert(20)
a.insert(15)
a.insert(5)
a.insert(35)
a.insert(25)
a.insert(30)

log((string)a.peek())
log((string)a.pop())
log((string)a.peek())
log((string)a.pop())
log((string)a.peek())
log((string)a.pop())
log((string)a.peek())
log((string)a.pop())
log((string)a.peek())
log((string)a.pop())
log((string)a.peek())
log((string)a.pop())
log((string)a.peek())
log((string)a.pop())

MaxHeap<float> b = MaxHeap<float>()
b.insert(10.5)
b.insert(20.5)
b.insert(15.5)
b.insert(5.5)
b.insert(35.5)
b.insert(25.5)
b.insert(30.5)

log((string)b.peek())
log((string)b.pop())
log((string)b.peek())
log((string)b.pop())
log((string)b.peek())
log((string)b.pop())
log((string)b.peek())
log((string)b.pop())
log((string)b.peek())
log((string)b.pop())
log((string)b.peek())
log((string)b.pop())
log((string)b.peek())
log((string)b.pop())

# 35
# 35
# 30
# 30
# 25
# 25
# 20
# 20
# 15
# 15
# 10
# 10
# 5
# 5
# 35.5
# 35.5
# 30.5
# 30.5
# 25.5
# 25.5
# 20.5
# 20.5
# 15.5
# 15.5
# 10.5
# 10.5
# 5.5
# 5.5