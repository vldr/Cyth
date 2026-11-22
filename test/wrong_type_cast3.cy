class Object
    int field

any Thing = Object | string

Thing myType = "hello world"

log(((Object)myType).field)

#> Invalid type cast.
#>   at <start>:8:7

#< RuntimeError: illegal cast