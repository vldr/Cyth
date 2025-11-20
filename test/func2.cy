import "env"
    void log(string n)
    void log(int n)

class A<K, T>
  void run(void(K, T) func, K a, T b)
    func(a, b)

void add(int a, float b)
  log("add " + a + " " + b + " = " + (a + b))

void product(int a, float b)
  log("product " + a + " " + b + " = " + (a * b))

A<int, float> a = A<int, float>()
a.run(add, 10, 10.5)
a.run(product, 10, 10.5)

# add 10 10.5 = 20.5
# product 10 10.5 = 105