import "env"
  void log(string n)

void(string)[] a

void hello(string n)
  log("hello " + n)

void world(string n)
  log("world " + n)

a.push(hello)
a.push(world)
a.push(log)

for int i = 0; i < a.length; i += 1
  a[i]("hi")

# hello hi
# world hi
# hi