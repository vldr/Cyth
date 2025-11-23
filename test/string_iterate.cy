import "env"
    void log(string n)
    void log(int n)

string a = "hello"
string b = "world"
string c = a + " " + b

for int i = 0; i < c.length; i += 1
    log("Index: " + i + ", Value: " + (int)c[i])

for int i = 0; i < c.length; i += 1
    log("Index: " + i + ", Value: " + c[i])

log(c)

# Index: 0, Value: 104
# Index: 1, Value: 101
# Index: 2, Value: 108
# Index: 3, Value: 108
# Index: 4, Value: 111
# Index: 5, Value: 32
# Index: 6, Value: 119
# Index: 7, Value: 111
# Index: 8, Value: 114
# Index: 9, Value: 108
# Index: 10, Value: 100
# Index: 0, Value: h
# Index: 1, Value: e
# Index: 2, Value: l
# Index: 3, Value: l
# Index: 4, Value: o
# Index: 5, Value:  
# Index: 6, Value: w
# Index: 7, Value: o
# Index: 8, Value: r
# Index: 9, Value: l
# Index: 10, Value: d
# hello world