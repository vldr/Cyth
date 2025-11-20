import "env"
    void log(string n)
    void log(int n)

class A
    int outer = 10
  
    void hello(int n)
        if n > 2
            int hi(int n)
              int hi(int n)
                log("-1 hi " + outer)
                return outer

              hi(10)

              log("0 hi")
              return n * 2

            hi(10)


        int hi(int n)
            log("1 hi")
            return n * 2

        hi(10)

        for int i = 0; i < 2; i += 1
            int hi(int n)
                log("2 hi")

                return n * 4

            hi(10)

            for int i = 0; i < 2; i += 1
                int hi(int n)
                    log("3 hi")

                    return n * 3

                hi(10)

int outer = 12
void hello(int n)
    int hi(int n)
        log("1 hi " + outer)
        return n * 2

    hi(10)

    for int i = 0; i < 2; i += 1
        int hi(int n)
            log("2 hi")

            return n * 4

        hi(10)

        for int i = 0; i < 2; i += 1
            int hi(int n)
                log("3 hi")

                return n * 3

            hi(10)

int hi(int n)
    log("global scope hi")
    return n * 3

A().hello(10)
hello(10)

for int i = 0; i < 2; i += 1
    hi(12)

    int hi(int n)
        log("top scope hi")
        return n * 3

    hi(10)

# -1 hi 10
# 0 hi
# 1 hi
# 2 hi
# 3 hi
# 3 hi
# 2 hi
# 3 hi
# 3 hi
# 1 hi 12
# 2 hi
# 3 hi
# 3 hi
# 2 hi
# 3 hi
# 3 hi
# global scope hi
# top scope hi
# global scope hi
# top scope hi