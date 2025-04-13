import "env"
    void log(string n)

class GameOfLife
  int width
  int height
  int[][] cells

  void __init__(int width, int height)
    this.width = width
    this.height = height

    for int x = 0; x < width; x += 1
      int[] row

      for int y = 0; y < height; y += 1
        row.push(0)

      cells.push(row)
                
  void addCell(int x, int y)
    cells[x][y] = 1

  int countNeighbors(int x, int y)
    int count = 0
    for int dx = -1; dx <= 1; dx += 1
      for int dy = -1; dy <= 1; dy += 1
        if dx == 0 and dy == 0
          continue

        if x + dx < 0 or y + dy < 0
          continue

        if x + dx >= width or y + dy >= height
          continue

        if cells[x + dx][y + dy]
          count = count + 1
  
    return count

  void nextGeneration()
    int[][] newCells

    for int x = 0; x < width; x += 1
      int[] row

      for int y = 0; y < height; y += 1
        row.push(0)

      newCells.push(row)

    for int x = 0; x < width; x += 1
      for int y = 0; y < height; y += 1
        int neighbors = countNeighbors(x, y)
        if cells[x][y]
          if neighbors == 2 or neighbors == 3
            newCells[x][y] = 1
        else
          if neighbors == 3
            newCells[x][y] = 1

    cells = newCells

  void printBoard()
    for int x = 0; x < width; x += 1
      string buffer
    
      for int y = 0; y < height; y += 1
        if cells[x][y]
          buffer += "✅ "
        else
          buffer += "❌ "

      log(buffer)

    log("")

GameOfLife game = GameOfLife(10, 9)
game.addCell(5, 3)
game.addCell(5, 4)
game.addCell(4, 4)
game.addCell(4, 5)

for int i = 0; i < 3; i += 1
    game.printBoard()
    game.nextGeneration()

# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ✅ ✅ ❌ ❌ ❌ 
# ❌ ❌ ❌ ✅ ✅ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ✅ ✅ ✅ ❌ ❌ ❌ 
# ❌ ❌ ❌ ✅ ✅ ✅ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ✅ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ✅ ❌ ✅ ❌ ❌ ❌ 
# ❌ ❌ ❌ ✅ ❌ ✅ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ✅ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
# ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ ❌ 
#