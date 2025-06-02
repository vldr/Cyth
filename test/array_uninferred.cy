[][0]
[].push(10)
[].pop()
[].reserve(100)

[[]][0]
[[]].push(10)
[[]].pop()
[[]].reserve(100)

void test()
[test()]

#! 1:1-1:3 The array type is uninferred so it cannot be used here.
#! 2:1-2:3 The array type is uninferred so it cannot be used here.
#! 3:1-3:3 The array type is uninferred so it cannot be used here.
#! 4:1-4:3 The array type is uninferred so it cannot be used here.
#! 6:1-6:5 The array type is uninferred so it cannot be used here.
#! 7:1-7:5 The array type is uninferred so it cannot be used here.
#! 8:1-8:5 The array type is uninferred so it cannot be used here.
#! 9:1-9:5 The array type is uninferred so it cannot be used here.
#! 12:2-12:8 The type cannot be void here.