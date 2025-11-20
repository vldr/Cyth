import "env"
    void log(string n)
    void log(int n)

string[] a = ["hello", "world", "how are you"]
for string a in a
    log(a)

# hello
# world
# how are you

for string a in ["hello", "world", "how are you"]
    log(a)

# hello
# world
# how are you

for char a in "helloworld"
    log((string)a)

# h
# e
# l
# l
# o
# w
# o
# r
# l
# d

for string a in ["abc", "def"]
    log(a + " -> " + it)

    for char a in a
        log(a + " -> " + it)

# abc -> 0
# a -> 0
# b -> 1
# c -> 2
# def -> 1
# d -> 0
# e -> 1
# f -> 2

class Simon
    int length = 3
    int counter = 0

    string __get__(int index)
        return "Simon says " + (counter += 1)

for string a in Simon()
    log(a)

# Simon says 1
# Simon says 2
# Simon says 3

Simon simon = Simon()
for string a in simon
    log(a)

# Simon says 1
# Simon says 2
# Simon says 3

for int a in [1, 2, 3]
    log((string)a)

# 1
# 2
# 3