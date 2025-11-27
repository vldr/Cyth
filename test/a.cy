import "env"
  void log(int n)
  void log(string n)

int height = 600
int width = 600

for int x = 0; x < width; x += 1
  for int y = 0; y < height; y += 1
    float map(float value, float inMin, float inMax, float outMin, float outMax)
      return outMin + (outMax - outMin) * (value - inMin) / (inMax - inMin)
