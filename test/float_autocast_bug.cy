import "env"
    void log(string n)
    void log(int n)

Raytracer raytracer = Raytracer(10, 10)
raytracer.draw()

float sqrt(float x)
    float z = 1 
    for int i = 1; i <= 20; i += 1
        z -= (z*z - x) / (2*z)
    return z
    
float pow(float x, float exp)
  if x == 0
    if exp > 0
      return 0
    return 1

  float diffExp = exp - (float)((int)exp)
  if diffExp < 0
    diffExp = -diffExp

  if diffExp < 0.000001
    int e = (int)exp
    bool negativeExp = false
    if e < 0
      e = -e
      negativeExp = true
    float result = 1
    float base = x
    for int i = e; i > 0; i = i / 2
      if i % 2 == 1
        result = result * base
      base = base * base
    if negativeExp
      return 1 / result
    return result

  if x < 0
    return 0

  float exp_series(float y)
    float sum = 1
    float term = 1
    for int i = 1; i <= 40; i += 1
      term = term * (y / (float)i)
      sum = sum + term
    return sum

  float ln_x = 0
  for int i = 0; i < 60; i += 1
    float ey = exp_series(ln_x)
    ln_x = ln_x - 1 + x / ey

  return exp_series(exp * ln_x)


class Raytracer
  int width
  int height
  
  Sphere[] spheres
  Light[] lights
  Vector cameraOrigin

  void __init__(int w, int h)
    width = w
    height = h

    cameraOrigin = Vector(0, 0, -1)
    
    Material matRed = Material(Vector(1, 0, 0), 0.1, 0.9, 0.5, 32.0)
    Material matSilver = Material(Vector(0, 0, 0), 0, 0, 0.8, 64.0)
    Material matGreen = Material(Vector(0, 1, 0), 0.1, 0.8, 0.2, 10.0)
    Material matGround = Material(Vector(0.8, 0.8, 0.8), 0.2, 0.8, 0, 0)

    spheres.push(Sphere(Vector(0, 0, 3.0), 1, matRed))
    spheres.push(Sphere(Vector(-1.75, -0.5, 4.0), 0.5, matSilver))
    spheres.push(Sphere(Vector(2.5, 0.5, 5.0), 1.5, matGreen))
    spheres.push(Sphere(Vector(0, -101.0, 3.0), 100.0, matGround)) 
    
    lights.push(Light(Vector(-5.0, 5.0, -5.0), Vector(1, 1, 1)))
    lights.push(Light(Vector(5.0, 2.0, -3.0), Vector(0.5, 0.5, 1)))

  void draw()
    for int y = 0; y < height; y += 1
      for int x = 0; x < width; x += 1    
        float u = (float)x / (float)width - 0.5
        float v = (float)(height - y) / (float)height - 0.5 
        
        Vector rayDirection = Vector(u, v, 1).normalized()
        Ray primaryRay = Ray(cameraOrigin, rayDirection)

        Vector color = trace(primaryRay, 5) 

        int r = (int)(clamp(color.x, 0, 1) * 255)
        int g = (int)(clamp(color.y, 0, 1) * 255)
        int b = (int)(clamp(color.z, 0, 1) * 255)
        
        log(r + " " + g + " " + b)    
  
  Vector trace(Ray ray, int depth)
    if depth <= 0
      return Vector(0, 0, 0) 

    HitRecord closestHit = HitRecord(-1, Vector(0, 0, 0), Vector(0, 0, 0), null)
    float closestSoFar = 10000.0 

    for int i = 0; i < spheres.length; i += 1
      HitRecord rec = spheres[i].hit(ray, 0.001, closestSoFar)
      if rec.t > 0
        closestSoFar = rec.t
        closestHit = rec

    if closestHit.t > 0
      return calculateLighting(closestHit, ray, depth)
    else
      Vector unit_direction = ray.direction.normalized()
      float t = 0.5 * (unit_direction.y + 1)
      return Vector(1, 1, 1).scale(1 - t) + Vector(0.5, 0.7, 1).scale(t)

  
  Vector calculateLighting(HitRecord rec, Ray ray, int depth)
    Vector finalColor = rec.material.color.scale(rec.material.ambient)  
    Vector viewDir = (ray.origin - rec.p).normalized()

    for int i = 0; i < lights.length; i += 1
      Vector lightDir = (lights[i].position - rec.p).normalized()
      
      Ray shadowRay = Ray(rec.p, lightDir)
      bool inShadow = false
      for int j = 0; j < spheres.length; j += 1
        HitRecord shadowRec = spheres[j].hit(shadowRay, 0.001, 10000.0)
        if shadowRec.t > 0
          inShadow = true
          break 
      
      if not inShadow
        float diff = max(0, rec.normal.dot(lightDir))
        Vector diffuse = rec.material.color.scale(diff * rec.material.diffuse)
        
        Vector reflectDir = lightDir.scale(-1) - rec.normal.scale(2 * lightDir.scale(-1).dot(rec.normal))
        float spec = pow(max(0, reflectDir.dot(viewDir)), rec.material.shininess)
        Vector specular = lights[i].color.scale(spec * rec.material.specular)

        finalColor = finalColor + diffuse + specular
    
    if rec.material.specular > 0.1 and rec.material.shininess > 0
        Vector reflectDir = ray.direction - rec.normal.scale(2 * ray.direction.dot(rec.normal))
        Ray reflectedRay = Ray(rec.p, reflectDir)
        Vector reflectionColor = trace(reflectedRay, depth - 1)
        finalColor = finalColor + reflectionColor.scale(rec.material.specular)

    return finalColor

class Ray
  Vector origin
  Vector direction

  void __init__(Vector origin, Vector direction)
    this.origin = origin
    this.direction = direction.normalized() 
  
  Vector at(float t)
    return origin + direction.scale(t)

class Material
  Vector color      
  float ambient    
  float diffuse    
  float specular   
  float shininess  

  void __init__(Vector color, float ambient, float diffuse, float specular, float shininess)
    this.color = color
    this.ambient = ambient
    this.diffuse = diffuse
    this.specular = specular
    this.shininess = shininess

class Light
  Vector position
  Vector color

  void __init__(Vector position, Vector color)
    this.position = position
    this.color = color

class HitRecord
  float t          
  Vector p         
  Vector normal    
  Material material 

  void __init__(float t, Vector p, Vector normal, Material material)
    this.t = t
    this.p = p
    this.normal = normal
    this.material = material

class Sphere
  Vector center
  Material material
  float radius

  void __init__(Vector center, float radius, Material material)
    this.center = center
    this.radius = radius
    this.material = material  
  
  HitRecord hit(Ray r, float t_min, float t_max)
    Vector oc = r.origin - center
    float a = r.direction.dot(r.direction)
    float b = 2 * oc.dot(r.direction)
    float c = oc.dot(oc) - radius * radius
    float discriminant = b*b - 4 * a * c

    if discriminant > 0
      float temp = (-b - sqrt(discriminant)) / (2 * a)
      if temp < t_max and temp > t_min
        Vector hitPoint = r.at(temp)
        Vector normal = (hitPoint - center).normalized()
        return HitRecord(temp, hitPoint, normal, material)
      
      temp = (-b + sqrt(discriminant)) / (2 * a)
      if temp < t_max and temp > t_min
        Vector hitPoint = r.at(temp)
        Vector normal = (hitPoint - center).normalized()
        return HitRecord(temp, hitPoint, normal, material)

    return HitRecord(-1, Vector(0,0,0), Vector(0,0,0), material)

class Vector
  float x
  float y
  float z

  void __init__(float x, float y, float z)
    this.x = x
    this.y = y
    this.z = z

  Vector __add__(Vector other)
    return Vector(x + other.x, y + other.y, z + other.z)

  Vector __sub__(Vector other)
    return Vector(x - other.x, y - other.y, z - other.z)

  float lengthSquared()
    return x * x + y * y + z * z

  float length()
    return sqrt(lengthSquared())
    
  Vector normalized()
    float l = length()
    if l > 0
      return Vector(x / l, y / l, z / l)
    return Vector(0, 0, 0)
  
  Vector scale(float s)
    return Vector(x * s, y * s, z * s)
 
  float dot(Vector other)
    return x * other.x + y * other.y + z * other.z
  
  Vector cross(Vector other)
    return Vector(
      y * other.z - z * other.y,
      z * other.x - x * other.z,
      x * other.y - y * other.x
    )

float clamp(float val, float min, float max)
  if val < min
    return min
  
  if val > max
    return max

  return val

float max(float a, float b)
  if a > b
      return a
  return b

# 165 201 255
# 164 200 255
# 163 200 255
# 163 199 255
# 162 199 255
# 162 199 255
# 162 199 255
# 163 199 255
# 163 200 255
# 164 200 255
# 169 203 255
# 169 203 255
# 168 203 255
# 167 202 255
# 167 202 255
# 167 202 255
# 167 202 255
# 167 202 255
# 168 203 255
# 169 203 255
# 174 206 255
# 174 206 255
# 173 206 255
# 173 205 255
# 173 205 255
# 172 205 255
# 173 205 255
# 173 205 255
# 30 232 51
# 25 255 51
# 180 210 255
# 179 209 255
# 179 209 255
# 178 209 255
# 255 92 127
# 255 89 127
# 255 92 127
# 33 204 51
# 30 255 52
# 47 255 69
# 185 213 255
# 185 213 255
# 185 213 255
# 255 100 127
# 255 127 157
# 255 96 128
# 255 99 132
# 255 100 127
# 23 255 30
# 62 255 75
# 191 216 255
# 191 216 255
# 191 216 255
# 255 108 127
# 255 109 128
# 255 108 127
# 255 125 161
# 255 108 127
# 8 75 8
# 38 255 38
# 196 220 255
# 148 170 204
# 70 50 50
# 255 92 92
# 255 102 102
# 255 105 105
# 255 104 104
# 255 94 94
# 18 44 18
# 20 171 20
# 127 127 127
# 126 126 126
# 40 40 40
# 124 124 124
# 181 96 96
# 232 98 98
# 223 97 97
# 96 96 96
# 98 98 98
# 99 99 99
# 192 192 192
# 193 193 193
# 193 193 193
# 194 194 194
# 195 195 195
# 195 195 195
# 195 195 195
# 196 196 196
# 196 196 196
# 196 196 196
# 204 204 204
# 204 204 204
# 205 205 205
# 205 205 205
# 206 206 206
# 206 206 206
# 207 207 207
# 207 207 207
# 207 207 207
# 208 208 208