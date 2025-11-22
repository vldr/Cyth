import "env"
    void log(string n)
    void log(int n)

log((string)"applesauce".startsWith("apple"))
# true

log((string)"applesauce".startsWith("app"))
# true

log((string)"applesauce".startsWith("applesauce"))
# true

log((string)"applesauce".startsWith(""))
# true

log((string)"applesauce".startsWith("sauce"))
# false

log((string)"applesauce".startsWith("Apple"))
# false

log((string)"applesauce".startsWith("apple"))
# true

log((string)"applesauce".startsWith("apple"))
# true

log((string)"".startsWith(""))
# true

log((string)"".startsWith("a"))
# false

log((string)"a".startsWith(""))
# true

log((string)"a".startsWith("a"))
# true

log((string)"a".startsWith("aa"))
# false

log((string)"banana".startsWith("ban"))
# true

log((string)"banana".startsWith("nan"))
# false

log((string)"hello".startsWith("he"))
# true

log((string)"hello".startsWith("lo"))
# false

log((string)"CaseTest".startsWith("casetest"))
# false

log((string)"mixCase".startsWith("mix"))
# true

log((string)"mixCase".startsWith("Mix"))
# false

log((string)"   leading".startsWith(" "))
# true

log((string)"tab\tchar".startsWith("tab\t"))
# true

log((string)"repeatrepeat".startsWith("repeat"))
# true

log((string)"repeatrepeat".startsWith("peat"))
# false

log((string)"😀😃😄".startsWith("😀"))
# true

log((string)"😀😃😄".startsWith("😃"))
# false

log((string)"𠮷𠮷".startsWith("𠮷"))
# true

log((string)"straße".startsWith("stra"))
# true

log((string)"straße".startsWith("strasse"))
# false

log((string)"👍👍👍".startsWith("👍👍"))
# true

log((string)"".startsWith(""))
# true

log((string)"abc".startsWith(""))
# true

log((string)"abc".startsWith("abcd"))
# false

log((string)"abc".startsWith("abc"))
# true

log((string)"abc".startsWith("ab"))
# true

log((string)"abc".startsWith("abd"))
# false
