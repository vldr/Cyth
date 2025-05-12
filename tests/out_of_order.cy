import "env"
  void log(string n)

Cellular cellular = Cellular(50, 50, 500, 500)
cellular.addCell(5, 4)
cellular.addCell(6, 4)
cellular.addCell(7, 4)
cellular.addCell(7, 3)
cellular.addCell(6, 2)

class Cellular
  int width
  int height
  int windowWidth
  int windowHeight

  int[][] cells

  void __init__(int width, int height, int windowWidth, int windowHeight)
    this.width = width
    this.height = height

    this.windowWidth = windowWidth
    this.windowHeight = windowHeight

    for int x = 0; x < width; x += 1
      int[] row

      for int y = 0; y < height; y += 1
        row.push(0)

      cells.push(row)

  void addCell(int x, int y)
    cells[x][y] = 1