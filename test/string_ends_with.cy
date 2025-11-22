import "env"
    void log(string n)
    void log(int n)

log((string)"applesauce".endsWith("sauce"))
# true

log((string)"applesauce".endsWith("apple"))
# false

log((string)"applesauce".endsWith("applesauce"))
# true

log((string)"applesauce".endsWith(""))
# true

log((string)"applesauce".endsWith("auce"))
# true

log((string)"applesauce".endsWith("Apple"))
# false

log((string)"".endsWith(""))
# true

log((string)"".endsWith("a"))
# false

log((string)"a".endsWith("a"))
# true

log((string)"a".endsWith("aa"))
# false

log((string)"banana".endsWith("ana"))
# true

log((string)"banana".endsWith("nan"))
# false

log((string)"hello".endsWith("lo"))
# true

log((string)"hello".endsWith("he"))
# false

log((string)"repeatrepeat".endsWith("repeat"))
# true

log((string)"repeatrepeat".endsWith("peat"))
# true

log((string)"CaseTest".endsWith("test"))
# false

log((string)"mixCase".endsWith("Case"))
# true

log((string)"mixCase".endsWith("case"))
# false

log((string)"   trailing".endsWith("ing"))
# true

log((string)"tab\tchar".endsWith("\tchar"))
# true

log((string)"😀😃😄".endsWith("😄"))
# true

log((string)"😀😃😄".endsWith("😀"))
# false

log((string)"𠮷野家".endsWith("野家"))
# true

log((string)"𠮷𠮷".endsWith("𠮷"))
# true

log((string)"straße".endsWith("ße"))
# true

log((string)"straße".endsWith("ss"))
# false

log((string)"applesauce".endsWith("sauce"))
# true

log((string)"applesauce".endsWith("sauce"))
# true

log((string)"".endsWith(""))
# true

log((string)"abc".endsWith(""))
# true

log((string)"abc".endsWith("abcd"))
# false

log((string)"abc".endsWith("abc"))
# true

log((string)"abc".endsWith("bc"))
# true

log((string)"hello world".endsWith("world"))
# true
