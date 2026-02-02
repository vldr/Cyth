import "env"
    void log(string n)
    void log(int n)

class Entry<K, V>
  K key
  V value
  Entry<K, V> next

  void __init__(K key, V value, Entry<K, V> next)
    this.key = key
    this.value = value
    this.next = next

class Map<K, V>
  Entry<K, V>[] buckets
  int bucketCount
  int size

  void __init__()
    bucketCount = 16
    size = 0

    for int i = 0; i < bucketCount; i += 1
      buckets.push(null)

  void __set__(K key, V value)
    set(key, value)

  V __get__(K key)
    return get(key)

  int hash(K key)
    int hash = key.hash() % buckets.length

    if hash < 0
      hash = hash * -1
    
    return hash

  void set(K key, V value)
    int index = hash(key)
    Entry<K, V> head = buckets[index]
    Entry<K, V> current = head

    while current != null
      if current.key == key
        current.value = value
        return
      
      current = current.next
  
    Entry<K, V> newEntry = Entry<K, V>(key, value, head)
    buckets[index] = newEntry
    size = size + 1
  
    float threshold = 0.75
    if size > bucketCount * threshold
      resize()

  V get(K key)
    int index = hash(key)
    Entry<K, V> current = buckets[index]

    while current != null
      if current.key == key
        return current.value

      current = current.next
  
    current = null
    return current.value

  void remove(K key)
    int index = hash(key)
    Entry<K, V> current = buckets[index]
    Entry<K, V> prev = null

    while current != null
      if current.key == key
        if prev == null
          buckets[index] = current.next
        else
          prev.next = current.next
        
        size = size - 1
        return
    
      prev = current
      current = current.next

  void resize()
    Entry<K, V>[] oldBuckets = buckets
    bucketCount = bucketCount * 2
    size = 0

    for int i = 0; i < bucketCount; i += 1
      buckets.push(null)

    for int i = 0; i < oldBuckets.length; i += 1
      Entry<K, V> current = oldBuckets[i]
      while current != null
        Entry<K, V> nextEntry = current.next
        set(current.key, current.value)
        current = nextEntry

  int getSize()
    return size

Map<int, any> map = Map<int, any>()

class Object
    int data

    void __init__(int data)
        this.data = data

map.set(0, Object(12))
map.set(1, (int[]) [22, 44, 66])
map.set(2, "hello world")

map[3] = (any) Object(12)
map[4] = (any) (int[]) [22, 44, 66]
map[5] = (any) "hello world"

Object a = (Object) map[0]
int[] b = (int[]) map[1]
string c = (string) map[2]

Object d = (Object) map[3]
int[] e = (int[]) map[4]
string f = (string) map[5]

log((string) a.data)
log((string) b[0])
log((string) b[1])
log((string) b[2])
log(c)

log((string) d.data)
log((string) e[0])
log((string) e[1])
log((string) e[2])
log(f)

# 12
# 22
# 44
# 66
# hello world
# 12
# 22
# 44
# 66
# hello world