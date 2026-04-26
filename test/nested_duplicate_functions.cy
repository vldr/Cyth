class A<T>
    void cool(int outerValue)
        void cool2(int value)
            log(value)

        cool2(outerValue)

A<int>().cool(10)
A<float>().cool(20)

# 10
# 20