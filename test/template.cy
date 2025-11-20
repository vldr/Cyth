import "env"
    void log(string n)
    void log(int n)

class Hashmap<K, V>
    K[] keys
    V[] values

    void __init__()
        for int i = 0; i < 100; i += 1
            this.keys.push(K())

        for int i = 0; i < 100; i += 1
            this.values.push(V())

    int hash(K key)
        return (int)key % this.keys.length

    V set(K key, V value)
        int index = hash(key)

        while this.keys[index] != K()
            if this.keys[index] == key
                this.values[index] = value
                return V()
    
            index = (index + 1) % this.keys.length
    
        this.keys[index] = key
        this.values[index] = value

        return get(key)

    bool contains(K key)
        return get(key) != V()

    V get(K key)
        int index = hash(key)

        while this.keys[index] != K()
            if this.keys[index] == key
                return this.values[index]

            index = (index + 1) % this.keys.length

        return V()

log(Hashmap<int, string>().set(10, "hello"))
log(Hashmap<char, string>().set('a', "world"))

# hello
# world