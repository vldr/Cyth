import "env"
    void log(bool n) 

class Hashmap
    char[] keys
    char[] values

    void __init__()
        for int i = 0; i < 100; i += 1
            this.keys.push((char)0)

        for int i = 0; i < 100; i += 1
            this.values.push((char)0)

    int hash(char key)
        return (int)key % this.keys.length

    void set(char key, char value)
        int index = hash(key)

        while this.keys[index] != (char)0
            if this.keys[index] == key
                this.values[index] = value
                return
    
            index = (index + 1) % this.keys.length
    
        this.keys[index] = key
        this.values[index] = value

    bool contains(char key)
        return get(key) != (char)0

    char get(char key)
        int index = hash(key)

        while this.keys[index] != (char)0
            if this.keys[index] == key
                return this.values[index]

            index = (index + 1) % this.keys.length

        return (char)0

bool isIsomorphic(string s, string t)
    if s.length != t.length
        return false

    Hashmap mapping_t = Hashmap()
    Hashmap mapping_s = Hashmap()

    for int i = 0; i < s.length; i += 1
        char s_char = s[i]
        char t_char = t[i]
        
        if mapping_s.contains(s_char)
            if mapping_s.get(s_char) != t_char
                return false
        else
            mapping_s.set(s_char, t_char)

        if mapping_t.contains(t_char)
            if mapping_t.get(t_char) != s_char
                return false
        else
            mapping_t.set(t_char, s_char)

    return true

log(isIsomorphic("egg", "add"))
log(isIsomorphic("foo", "bar"))
log(isIsomorphic("paper", "title"))
log(isIsomorphic("ab", "ab"))
log(isIsomorphic("a", "a"))

# 1
# 0
# 1
# 1
# 1