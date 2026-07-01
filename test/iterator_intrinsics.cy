log(((int[]) []).__begin__())

# 0

log(((int[]) [1,2,3]).__hasNext__(0))
log(((int[]) [1,2,3]).__hasNext__(1))
log(((int[]) [1,2,3]).__hasNext__(2))
log(((int[]) [1,2,3]).__hasNext__(3))

# 1
# 1
# 1
# 0

log(((int[]) [1,2,3]).__next__(0))
log(((int[]) [1,2,3]).__next__(1))
log(((int[]) [1,2,3]).__next__(2))
log(((int[]) [1,2,3]).__next__(3))

# 1
# 2
# 3
# 4

log("".__begin__())

# 0

log("abc".__hasNext__(0))
log("abc".__hasNext__(1))
log("abc".__hasNext__(2))
log("abc".__hasNext__(3))

# 1
# 1
# 1
# 0

log("abc".__next__(0))
log("abc".__next__(1))
log("abc".__next__(2))
log("abc".__next__(3))

# 1
# 2
# 3
# 4

