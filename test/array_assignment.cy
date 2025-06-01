import "env"
    void log(int n)

int[] array
array.push(10)
array.push(20)

int[] array2
array2.push(5)
array2.push(8)

int[][] array_of_array
array_of_array.push(array)
array_of_array.push(array2)
array_of_array.push(array)

array_of_array[0] = array_of_array[1]
array_of_array[1] = array_of_array.pop()

log(array_of_array[0][0])
log(array_of_array[0][1])

log(array_of_array[1][0])
log(array_of_array[1][1])

log(array[0])
log(array[1])

log(array2[0])
log(array2[1])

array[0] = 444
array[1] = 888
array2[0] = 111
array2[1] = 222

log(array_of_array[0][0])
log(array_of_array[0][1])

log(array_of_array[1][0])
log(array_of_array[1][1])

log(array[0])
log(array[1])

log(array2[0])
log(array2[1])

# 5
# 8
# 10
# 20
# 10
# 20
# 5
# 8
# 111
# 222
# 444
# 888
# 444
# 888
# 111
# 222