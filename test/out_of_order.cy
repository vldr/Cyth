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

    this.cells.reserve(width, height)

  void addCell(int x, int y)
    cells[x][y] = 1