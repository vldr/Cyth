class A<T>
    T func<T>()
        return 10

log(A<string>().func<int>())

# 10
