alias JsonValue = any
alias JsonBool = bool
alias JsonNumber = float
alias JsonInteger = int
alias JsonString = string
alias JsonArray = JsonValue[]
alias JsonObject = Map<JsonString, JsonValue>

string jsonEncode(JsonValue value)
  void jsonEncodeString(char[] buffer, string value)
    buffer.push('"')

    for char c in value
      if c == '"'
        buffer.push('\\')
        buffer.push('"')
      else if c == '\\'
        buffer.push('\\')
        buffer.push('\\')
      else if c == '\n'
        buffer.push('\\')
        buffer.push('n')
      else if c == '\r'
        buffer.push('\\')
        buffer.push('r')
      else if c == '\t'
        buffer.push('\\')
        buffer.push('t')
      else
        buffer.push(c)
    
    buffer.push('"') 

  void jsonEncode(char[] buffer, JsonValue value)
    match value
      case JsonBool
        buffer.pushString(value ? "true" : "false")

      case JsonNumber
        buffer.pushString((string)value)

      case JsonInteger
        buffer.pushString((string)value)

      case JsonString
        jsonEncodeString(buffer, value)

      case JsonArray
        buffer.push('[')

        for JsonValue value in value
          if it > 0
            buffer.push(',')
          
          jsonEncode(buffer, value)

        buffer.push(']')

      case JsonObject
        buffer.push('{')

        bool first = true
        for int index in value
          if not first
            buffer.push(',')

          jsonEncodeString(buffer, value.keys[index])
          buffer.push(':')
          jsonEncode(buffer, value.values[index])

          first = false

        buffer.push('}')
      
      default
        buffer.pushString("null")

  char[] buffer
  jsonEncode(buffer, value)

  return buffer.toString()

JsonArray array = JsonArray()
array.push('a')
array.push(10)
array.push(12.5)
array.push("hello world")
array.push(true)
array.push(false)

JsonObject root = JsonObject()
root["numbers"] = array

log(jsonEncode(root))

# {"numbers":[null,10,12.5,"hello world",true,false]}

class Map<K, V>
    K[] keys
    V[] values
    bool[] used
    int bucketCount
    int size

    void __init__()
        bucketCount = 32
        size = 0

        keys.reserve(bucketCount)
        values.reserve(bucketCount)
        used.reserve(bucketCount)
    
    void __set__(K key, V value)
        insertAndResize(key, value)

    V __get__(K key)
        return get(key)

    MapIterator __begin__()
        MapIterator iterator = MapIterator()
        while iterator.index < used.length and not used[iterator.index]
            iterator.index += 1
        return iterator

    bool __hasNext__(MapIterator iterator)
        return iterator.index < used.length

    MapIterator __next__(MapIterator iterator)
        iterator.index += 1
        while iterator.index < used.length and not used[iterator.index]
            iterator.index += 1
        return iterator

    int __get__(MapIterator iterator)
        return iterator.index

    int hash(K key)
        int h = key.hash() % bucketCount
        if h < 0
            h *= -1
        return h

    void insert(K key, V value)
        int index = hash(key)

        while used[index]
            if keys[index] == key
                values[index] = value
                return
            index = (index + 1) % bucketCount

        keys[index] = key
        values[index] = value
        used[index] = true
        size += 1

    void insertAndResize(K key, V value)
        void resize()
            K[] oldKeys = keys
            V[] oldValues = values
            bool[] oldUsed = used
            int oldCount = bucketCount

            bucketCount = bucketCount * 2
            size = 0
            keys = []
            values = []
            used = []
            keys.reserve(bucketCount)
            values.reserve(bucketCount)
            used.reserve(bucketCount)

            for int i = 0; i < oldCount; i += 1
                if oldUsed[i]
                    insert(oldKeys[i], oldValues[i])

        insert(key, value)

        float threshold = 0.75
        if size > bucketCount * threshold
            resize()

    bool contains(K key)
        int index = hash(key)
        int start = index

        while used[index]
            if keys[index] == key
                return true
            index = (index + 1) % bucketCount
            if index == start
                return false

        return false

    V get(K key)
        int index = hash(key)
        int start = index

        while used[index]
            if keys[index] == key
                return values[index]
            index = (index + 1) % bucketCount
            if index == start
                break

        return V()

    void remove(K key)
        int index = hash(key)
        int start = index

        while used[index]
            if keys[index] == key
                used[index] = false
                size -= 1

                int next = (index + 1) % bucketCount
                while used[next]
                    K rehashKey = keys[next]
                    V rehashValue = values[next]
                    used[next] = false
                    size -= 1
                    insert(rehashKey, rehashValue)
                    next = (next + 1) % bucketCount

                return

            index = (index + 1) % bucketCount
            if index == start
                return

class MapIterator
    int index