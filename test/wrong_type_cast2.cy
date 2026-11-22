any Thing = int[] | string
Thing myType = "hello world"

log(((int[])myType).length)

#> Invalid type cast.
#>   at <start>:4:7

#< RuntimeError: illegal cast