class A
class B

any[] a
(A[])a
(any[])a

A i
B j

(B)i
(A)i
(any)i

(A)j
(B)j
(any)j

#! 5:2-5:3 Invalid type conversion from 'any[]' to 'A[]'.
#! 11:2-11:3 Invalid type conversion from 'A' to 'B'.
#! 15:2-15:3 Invalid type conversion from 'B' to 'A'.