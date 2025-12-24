class Test<K>
    Test<Test<K>> member

Test<int>
Test<float> a

#! 2:5-2:9 Cannot instiantiate 'Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<float>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>' template, recursion limit reached. (occurred when creating Test<float> at 5:1)
#! 2:5-2:9 Cannot instiantiate 'Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<Test<int>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>' template, recursion limit reached. (occurred when creating Test<int> at 4:1)