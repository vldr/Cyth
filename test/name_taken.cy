class A
int A
A + 2

class B<T>
int B
B + 2

int C
string C
C


#! 2:5-2:6 The name 'A' already exists.
#! 6:5-6:6 The name 'B' already exists.
#! 10:8-10:9 The name 'C' already exists.
#! 3:3-3:4 Mismatched types, expected 'class A' but got 'int'.
#! 7:3-7:4 Mismatched types, expected 'class B<T>' but got 'int'.