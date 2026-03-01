int N = 50
float PI = 3.141592653589793
float SOLAR_MASS = 4 * PI * PI
float DAYS_PER_YEAR = 365.24

NBodySystem bodies = NBodySystem()

log((int)(bodies.energy() * 1000000))
# -169075

for int i=0; i < N; i += 1
    bodies.advance(0.01)

log((int)(bodies.energy() * 1000000))
# -169063

class NBodySystem 
  int LENGTH = 5

  Body[] bodies

  void __init__()
    bodies = [
        Body.sun(null),
        Body.jupiter(null),
        Body.saturn(null),
        Body.uranus(null),
        Body.neptune(null)
    ]

    float px = 0.0
    float py = 0.0
    float pz = 0.0
    for int i=0; i < LENGTH; i += 1
        px += bodies[i].vx * bodies[i].mass
        py += bodies[i].vy * bodies[i].mass
        pz += bodies[i].vz * bodies[i].mass
    
    bodies[0].offsetMomentum(px,py,pz)
  

  void advance(float dt) 
    Body[] b = bodies
    for int i=0; i < LENGTH-1; i += 1
        Body iBody = b[i]
        float iMass = iBody.mass
        float ix = iBody.x
        float iy = iBody.y
        float iz = iBody.z

        for int j=i+1; j < LENGTH; j += 1
          Body jBody = b[j]
          float dx = ix - jBody.x
          float dy = iy - jBody.y
          float dz = iz - jBody.z

          float dSquared = dx * dx + dy * dy + dz * dz
          float distance = dSquared.sqrt()
          float mag = dt / (dSquared * distance)

          float jMass = jBody.mass

          iBody.vx -= dx * jMass * mag
          iBody.vy -= dy * jMass * mag
          iBody.vz -= dz * jMass * mag

          jBody.vx += dx * iMass * mag
          jBody.vy += dy * iMass * mag
          jBody.vz += dz * iMass * mag

    for int i=0; i < LENGTH; i += 1
        Body body = b[i]
        body.x += dt * body.vx
        body.y += dt * body.vy
        body.z += dt * body.vz

  float energy()
    float dx
    float dy
    float dz
    float distance
    float e = 0.0

    for int i=0; i < bodies.length; i += 1
        Body iBody = bodies[i]
        e += 0.5 * iBody.mass * (iBody.vx * iBody.vx
                                + iBody.vy * iBody.vy
                                + iBody.vz * iBody.vz)

        for int j = i+1; j < bodies.length; j += 1
          Body jBody = bodies[j]
          dx = iBody.x - jBody.x
          dy = iBody.y - jBody.y
          dz = iBody.z - jBody.z

          distance = (dx*dx + dy*dy + dz*dz).sqrt()
          e -= (iBody.mass * jBody.mass) / distance 
    
    return e   

class Body 
  float x
  float y
  float z
  float vx
  float vy
  float vz
  float mass

  Body jupiter()
    Body p = Body()
    p.x = 4.841431442464721
    p.y = -1.1603200440274284
    p.z = -0.10362204447112311
    p.vx = 0.001660076642744037 * DAYS_PER_YEAR
    p.vy = 0.007699011184197404 * DAYS_PER_YEAR
    p.vz = -0.0000690460016972063023 * DAYS_PER_YEAR
    p.mass = 0.000954791938424326609 * SOLAR_MASS
    return p


  Body saturn()
    Body p = Body()
    p.x = 8.34336671824457987
    p.y = 4.12479856412430479
    p.z = -0.403523417114321381
    p.vx = -0.00276742510726862411 * DAYS_PER_YEAR
    p.vy = 0.00499852801234917238 * DAYS_PER_YEAR
    p.vz = 0.0000230417297573763929 * DAYS_PER_YEAR
    p.mass = 0.000285885980666130812 * SOLAR_MASS
    return p

  Body uranus()
    Body p = Body()
    p.x = 12.8943695621391310
    p.y = -15.1111514016986312
    p.z = -0.223307578892655734
    p.vx = 0.00296460137564761618 * DAYS_PER_YEAR
    p.vy = 0.00237847173959480950 * DAYS_PER_YEAR
    p.vz = -0.0000296589568540237556 * DAYS_PER_YEAR
    p.mass = 0.0000436624404335156298 * SOLAR_MASS
    return p

  Body neptune()
    Body p = Body()
    p.x = 15.3796971148509165
    p.y = -25.9193146099879641
    p.z = 0.179258772950371181
    p.vx = 0.00268067772490389322 * DAYS_PER_YEAR
    p.vy = 0.00162824170038242295 * DAYS_PER_YEAR
    p.vz = -0.0000951592254519715870 * DAYS_PER_YEAR
    p.mass = 0.0000515138902046611451 * SOLAR_MASS
    return p

  Body sun()
    Body p = Body()
    p.mass = SOLAR_MASS
    return p

  Body offsetMomentum(float px, float py, float pz)
    vx = -px / SOLAR_MASS
    vy = -py / SOLAR_MASS
    vz = -pz / SOLAR_MASS
    return this
  
