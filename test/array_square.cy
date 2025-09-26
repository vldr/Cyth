import "env"
  void log(int n)

int maxSquare(int[][] data)
  int m = data.length
  int n = data[0].length
  int[][] dp
  dp.reserve(m, n)

  int max

  for int i = 0; i < m; i += 1
    for int j = 0; j < n; j += 1
      if not data[i][j]
        continue

      int left = 0
      if i-1 >= 0
        left = dp[i-1][j]

      int top = 0
      if j-1 >= 0
        top = dp[i][j-1]

      int corner = 0
      if j-1 >= 0 and i-1 >= 0
        corner = dp[i-1][j-1]

      int min = left

      if top < min
        min = top

      if corner < min
        min = corner

      dp[i][j] = min + 1

      if dp[i][j] > max
        max = dp[i][j]

  return max * max

log(
  maxSquare([
    [1,1,1,1,1,1],
    [1,1,1,1,1,1],
    [1,0,1,1,1,1],
    [0,0,1,0,1,0]
  ])
)

log(
  maxSquare([
    [1,0,1,0,0],
    [1,0,1,1,1],
    [1,1,1,1,1],
    [1,0,0,1,0]
  ])
)

# 9
# 4