import "env"
    void log(string n)
    void log(int n)

log((string)"applesauce".count("a"))
# 2

log((string)"applesauce".count("p"))
# 2

log((string)"applesauce".count("apple"))
# 1

log((string)"applesauce".count("sauce"))
# 1

log((string)"applesauce".count("z"))
# 0

log((string)"applesauce".count(""))
# 11

log((string)"".count(""))
# 1

log((string)"".count("a"))
# 0

log((string)"aaaaa".count("aa"))
# 2

log((string)"banana".count("ana"))
# 1

log((string)"banana".count("na"))
# 2

log((string)"banana".count("nana"))
# 1

log((string)"banana".count("ban"))
# 1

log((string)"banana".count("bana"))
# 1

log((string)"banana".count("banana"))
# 1

log((string)"banana".count("banana!"))
# 0

log((string)"abcabcabc".count("abc"))
# 3

log((string)"abcabcabc".count("bca"))
# 2

log((string)"abcabcabc".count("cab"))
# 2

log((string)"abcabcabc".count("abcd"))
# 0

log((string)"😀😃😀😃😀".count("😀"))
# 3

log((string)"😀😃😀😃😀".count("😃"))
# 2

log((string)"𠮷𠮷𠮷".count("𠮷"))
# 3

log((string)"👍👍👍".count("👍"))
# 3

log((string)"straße".count("ß"))
# 1

log((string)"straße".count("ss"))
# 0

log((string)"tab\tchar\t".count("\t"))
# 2

log((string)"a\nb\nc".count("\n"))
# 2

log((string)"appleapple".count("apple"))
# 2

log((string)"apple".count("apple"))
# 1

log((string)"apple".count("applee"))
# 0

