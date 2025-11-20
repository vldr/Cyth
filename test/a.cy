import "env"
  void log(string n)
  void log(int n)
  
void merge(int[] nums1, int m, int[] nums2, int n)
  int left = m - 1
  int right = n - 1
  int index = nums1.length - 1

  while left >= 0 or right >= n
    if left >= 0 and right >= 0
      if nums1[left] > nums2[right]
        nums1[index] = nums1[left]
        left -= 1
      else
        nums1[index] = nums2[right]
        right -= 1
    else if left >= 0
      nums1[index] = nums1[left]
      left -= 1
    else
      nums1[index] = nums2[right]
      right -= 1

    index -= 1

int[] input = [1,2,3,0,0,0]
merge(input, 3, [2,5,6], 3)
log((string)input)

input = [2,5,6,0,0,0]
merge(input, 3, [1,2,3], 3)
log((string)input)

input = [1]
merge(input, 1, [], 0)
log((string)input)

input = [0]
merge(input, 0, [1], 1)
log((string)input)