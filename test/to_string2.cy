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
  int bucketCount
  int size
  Entry<K, V>[] buckets

  void __init__()
    bucketCount = 16
    size = 0

    for int i = 0; i < bucketCount; i += 1
      buckets.push(null)

  void __set__(K key, V value)
    insert(key, value)

  V __get__(K key)
    return get(key)

  int hash(K key)
    int hash = key.hash() % buckets.length

    if hash < 0
      hash = hash * -1
    
    return hash

  void insert(K key, V value)
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
        insert(current.key, current.value)
        current = nextEntry

  int getSize()
    return size

class Test
    int value

    int hash()
        return value

    string __str__()
        return "My value: " + value

Test t1 = Test()
t1.value = 1

Test t2 = Test()
t2.value = 2

Map<Test, string> a = Map<Test, string>()
a.insert(t1, "hello")
a.insert(t2, "world")

log("a: " + a)

Map<string, string> b = Map<string, string>()
b.insert("10.0", "100")
b.insert("26.0", "200")
b.insert("costarring", "200")
b.insert("liquid", "100")

log("b: " + b)


Map<Test, string> c = Map<Test, string>()
c[t1] = "hello"
c[t2] = "world"

log("c: " + c)

Map<string, string> d = Map<string, string>()
d["10.0"] = "100"
d["26.0"] = "200" 
d["costarring"] = "200"
d["liquid"] = "100"

log("d: " + d)

# a: Map<Test, string>(\n bucketCount = 16,\n size = 2,\n buckets = [\n  null,\n  Entry<Test, string>$1(\n   key = My value: 1,\n   value = hello,\n   next = null\n  ),\n  Entry<Test, string>$2(\n   key = My value: 2,\n   value = world,\n   next = null\n  ),\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null\n ]\n)
# b: Map<string, string>(\n bucketCount = 16,\n size = 4,\n buckets = [\n  null,\n  Entry<string, string>$1(\n   key = 26.0,\n   value = 200,\n   next = null\n  ),\n  null,\n  null,\n  null,\n  null,\n  Entry<string, string>$2(\n   key = 10.0,\n   value = 100,\n   next = null\n  ),\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  Entry<string, string>$3(\n   key = liquid,\n   value = 100,\n   next = Entry<string, string>$4(\n    key = costarring,\n    value = 200,\n    next = null\n   )\n  ),\n  null,\n  null\n ]\n)
# c: Map<Test, string>(\n bucketCount = 16,\n size = 2,\n buckets = [\n  null,\n  Entry<Test, string>$1(\n   key = My value: 1,\n   value = hello,\n   next = null\n  ),\n  Entry<Test, string>$2(\n   key = My value: 2,\n   value = world,\n   next = null\n  ),\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  null\n ]\n)
# d: Map<string, string>(\n bucketCount = 16,\n size = 4,\n buckets = [\n  null,\n  Entry<string, string>$1(\n   key = 26.0,\n   value = 200,\n   next = null\n  ),\n  null,\n  null,\n  null,\n  null,\n  Entry<string, string>$2(\n   key = 10.0,\n   value = 100,\n   next = null\n  ),\n  null,\n  null,\n  null,\n  null,\n  null,\n  null,\n  Entry<string, string>$3(\n   key = liquid,\n   value = 100,\n   next = Entry<string, string>$4(\n    key = costarring,\n    value = 200,\n    next = null\n   )\n  ),\n  null,\n  null\n ]\n)