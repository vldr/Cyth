import "env"
  void log(int n)
  void log(string n)

Game game = Game(25, 25, 500, 500)
log((string)game)

# void keyPressed(char key)
#   game.input(key)

# void draw(int time)
#   game.update(time)
#   game.draw()

class Game
  float ticksPerSecond
  float tickLength
  float delta
  int deltaTicks
  int lastTime

  int height
  int width
  Vector resolution

  Snake snake
  Apple apple

  void __init__(int width, int height, int windowWidth, int windowHeight)
    this.ticksPerSecond = 10.0
    this.tickLength = 1000.0 / ticksPerSecond

    this.width = width
    this.height = height
    this.resolution = Vector(windowWidth / width, windowHeight / height)

    this.snake = Snake(width / 2 * resolution.x, (height - 1) * resolution.y)
    this.apple = Apple(width / 2 * resolution.x, height / 2 * resolution.y)


  void update(int time)
    delta += (float)(time - lastTime) / tickLength
    deltaTicks = (int)delta
    delta -= (float)deltaTicks
    lastTime = time

    for int i = 0; i < deltaTicks; i += 1
      snake.update()
      apple.update()

  void draw()
    for int x = 0; x < width; x += 1
      for int y = 0; y < height; y += 1
        int color = 30
        if (x + y) % 2 == 0
          color = 20


    apple.draw()
    snake.draw()

  void input(char key)
    if snake.dead
      return

    char up = (char)38
    char down = (char)40
    char left = (char)37
    char right = (char)39

    if key == up and (snake.body.length == 1 or snake.body[0].y <= snake.body[1].y)
      snake.direction.x = 0
      snake.direction.y = -1
    else if key == down and (snake.body.length == 1 or snake.body[0].y >= snake.body[1].y)
      snake.direction.x = 0
      snake.direction.y = 1
    else if key == left and (snake.body.length == 1 or snake.body[0].x <= snake.body[1].x)
      snake.direction.x = -1
      snake.direction.y = 0
    else if key == right and (snake.body.length == 1 or snake.body[0].x >= snake.body[1].x)
      snake.direction.x = 1
      snake.direction.y = 0

  int hsvToRgb(float h, float s, float v) 
    float r
    float g
    float b
    
    int i = (int)(h * 6)
    float f = h * 6 - i
    float p = v * (1 - s)
    float q = v * (1 - f * s)
    float t = v * (1 - (1 - f) * s)

    i %= 6

    if i == 0
      r = v
      g = t
      b = p
    else if i == 1
      r = q
      g = v
      b = p
    else if i == 2
      r = p
      g = v
      b = t
    else if i == 3
      r = p
      g = q
      b = v
    else if i == 4
      r = t
      g = p
      b = v
    else
      r = v
      g = p
      b = q

    int rgb
    rgb |= (int)(r * 255) << 16
    rgb |= (int)(g * 255) << 8
    rgb |= (int)(b * 255)

    return rgb

class Snake
  Vector[] body
  Vector[] oldBody

  Vector direction
  bool dead

  void __init__(int x, int y)
    direction = Vector(0, -1)
    body.push(Vector(x, y))
    oldBody.push(Vector(x, y))

  void update()
    if dead
      body.pop()
      oldBody.pop()

      if body.length > 1
        return
      
      dead = false
  
    for int i = -1; i < body.length; i += 1
      oldBody[i].x = body[i].x
      oldBody[i].y = body[i].y

    for int i = body.length - 1; i > 0; i -= 1
      if body[i] == body[0]
        game.apple.hue = 0.0
        dead = true
        return

    for int i = body.length - 1; i > 0; i -= 1
      body[i].x = body[i - 1].x
      body[i].y = body[i - 1].y

    body[0] += direction * game.resolution

    if body[0] == game.apple.position
      body.push(body[0] + direction * game.resolution)
      oldBody.push(body[0].clone())

    if body[0].x < 0
      body[0].x = (game.width - 1) * game.resolution.x
    else if body[0].x >= game.width * game.resolution.x
      body[0].x = 0
    
    if body[0].y < 0
      body[0].y = (game.height - 1) * game.resolution.y
    else if body[0].y >= game.height * game.resolution.y
      body[0].y = 0

  void draw()
    for int i = 0; i < body.length; i += 1
      if dead
      else if i == 0
      else
        int rgb = game.hsvToRgb((i - 1) * 0.05, 1.0, 1.0)
        

      Vector difference = body[i] - oldBody[i]
      if difference.x == game.width * game.resolution.x - game.resolution.x
        difference = Vector(-game.resolution.x, 0)
      if difference.x == -game.width * game.resolution.x + game.resolution.x
        difference = Vector(game.resolution.x, 0)
      if difference.y == game.height * game.resolution.y - game.resolution.y
        difference = Vector(0, -game.resolution.y)
      if difference.y == -game.height * game.resolution.y + game.resolution.y
        difference = Vector(0, game.resolution.y)

      difference.x = (int)(difference.x * game.delta)
      difference.y = (int)(difference.y * game.delta)

      Vector position = oldBody[i] + difference

class Apple
  Vector position
  float hue

  void __init__(int x, int y)
    position = Vector(x, y)

  void update()
    if position == game.snake.body[0]
      int randomX = (int)((game.width - 1)) * game.resolution.x
      int randomY = (int)((game.height - 1)) * game.resolution.y
      
      position.x = randomX
      position.y = randomY
      hue += 0.05

  void draw()
    int rgb = game.hsvToRgb(hue, 1.0, 1.0)


class Vector
  int x
  int y

  void __init__(int x, int y)
    this.x = x
    this.y = y

  Vector __add__(Vector other)
    return Vector(x + other.x, y + other.y)

  Vector __sub__(Vector other)
    return Vector(x - other.x, y - other.y)

  Vector __mul__(Vector other)
    return Vector(x * other.x, y * other.y)

  bool __eq__(Vector other)
    return x == other.x and y == other.y

  string __str__()
    return "(" + x + ", " + y + ")"

  Vector clone()
    return Vector(x, y)