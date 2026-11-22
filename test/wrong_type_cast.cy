any Thing = int[] | string
Thing myType = (int[])[1,2,3]

log((string)myType)

#> Invalid type cast.
#>   at <start>:4:6

#< RuntimeError: illegal cast