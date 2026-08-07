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

Map<int, int> a 
a[0]

#> Invalid memory or null pointer access
#>   at Map<int, int>.hash.int(int):41:30
#>   at Map<int, int>.get.int(int):100:21
#>   at Map<int, int>.__get__.int(int):20:16
#>   at <start>:140:3

#< RuntimeError: dereferencing a null pointer