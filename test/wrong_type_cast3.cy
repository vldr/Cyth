class Object
    int field

any myType = "hello world"

log(((Object)myType).field)

#> Invalid type cast.
#>   at <start>:6:7

#< RuntimeError: illegal cast