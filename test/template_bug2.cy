class B<T>
    T field2 = "hello"

class A<T>
    T field = B<T>().field2

A<float> a = A<float>()
log("" + a)

#! 2:14-2:15 Mismatched types, expected 'float' but got 'string'.\n* occurred when creating B<float> at 5:15\n* occurred when creating A<float> at 7:1