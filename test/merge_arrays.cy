import "env"
  void log(string n)
  void log(int n)
  
void merge(int[] nums1, int m, int[] nums2, int n)
  int[] numstemp
  numstemp.reserve(m + n)

  int left = 0
  int right = 0
  int index = 0

  while left < m or right < n
    if left < m and right < n
      if nums1[left] < nums2[right]
        numstemp[index] = nums1[left]
        left += 1
      else
        numstemp[index] = nums2[right]
        right += 1
    else if left < m
      numstemp[index] = nums1[left]
      left += 1
    else
      numstemp[index] = nums2[right]
      right += 1

    index += 1

  for int i = 0; i < nums1.length; i += 1
    nums1[i] = numstemp[i]

int[] input = [1,2,3,0,0,0]
merge(input, 3, [2,5,6], 3)
log((string)input)

input = [1]
merge(input, 1, [], 0)
log((string)input)

input = [0]
merge(input, 0, [1], 1)
log((string)input)

# [1, 2, 2, 3, 5, 6]
# [1]
# [1]