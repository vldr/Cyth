class Object<T>
    int field = 67

    void __init__()
      T = T

void func<T>(T apple)
  Object<T>()

func<int>(0)

#! 5:9-5:10 The expression is not assignable.\n* occurred when creating Object<int> at 8:3\n* occurred when creating func<int> at 10:1