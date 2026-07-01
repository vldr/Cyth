int(int[]) a = ((int[]) []).__begin__
log(a([]))

# 0

bool(int[], int) b = ((int[]) []).__hasNext__
log(b([1,2,3], 0))
log(b([1,2,3], 1))
log(b([1,2,3], 2))
log(b([1,2,3], 3))

# 1
# 1
# 1
# 0

int(int[], int) c = ((int[]) []).__next__
log(c([1,2,3], 0))
log(c([1,2,3], 1))
log(c([1,2,3], 2))
log(c([1,2,3], 3))

# 1
# 2
# 3
# 4

int(string) d = "".__begin__
log(d(""))

# 0

bool(string, int) e = "".__hasNext__
log(e("abc", 0))
log(e("abc", 1))
log(e("abc", 2))
log(e("abc", 3))

# 1
# 1
# 1
# 0

int(string, int) f = "".__next__
log(f("abc", 0))
log(f("abc", 1))
log(f("abc", 2))
log(f("abc", 3))

# 1
# 2
# 3
# 4

