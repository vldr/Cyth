import "env"
    void log(int n)

bool generateDiff(int[] nums, int[][] queries, int k)
    int[] diff

    for int i = 0; i < nums.length; i += 1
        if i == 0
            diff.push(nums[i])
        else
            diff.push(nums[i] - nums[i - 1])

    for int i = 0; i < k; i += 1
        int l = queries[i][0]
        int r = queries[i][1]
        int val = queries[i][2]

        diff[l] = diff[l] - val

        if r + 1 < diff.length
            diff[r + 1] = diff[r + 1] + val
    
    for int i = 0; i < diff.length; i += 1
        if i != 0
            diff[i] = diff[i - 1] + diff[i]

        if diff[i] > 0
            return false
        
    return true

int minArray(int[] nums, int[][] queries)
    if not generateDiff(nums, queries, queries.length)
        return -1

    int left = 0
    int right = queries.length

    while left <= right
        int middle = (left + right) / 2
        
        if generateDiff(nums, queries, middle)
            right = middle - 1
        else
            left = middle + 1


    return left

log(
    minArray([2,0,2], [[0,2,1],[0,2,1],[0,2,1],[1,1,3]])
)

log(
    minArray([4,3,2,1], [[1,3,2],[0,2,1]])
)

log(
    minArray([0], [[0,0,2], [0,0,4], [0,0,4], [0,0,3], [0,0,5]])
)

# 2
# -1
# 0