any Thing = A | B

class A
class B

Thing[] a
(A[])a
(Thing[])a

A i
B j

(B)i
(A)i
(Thing)i

(A)j
(B)j
(Thing)j

#! 7:2-7:3 Invalid type conversion from 'Thing[]' to 'A[]'.
#! 13:2-13:3 Invalid type conversion from 'A' to 'B'.
#! 17:2-17:3 Invalid type conversion from 'B' to 'A'.