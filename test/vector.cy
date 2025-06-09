class Vector
  float x
  float y
  float z
  
  float dot(Vector o)
    return x * o.x + y * o.y + z * o.z

  float length()
    return dot(this).sqrt()

float(Vector) func = Vector().length
func(Vector()).sqrt()
