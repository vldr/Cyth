int i = 0
for ;;
    if i == 3
        break
    
    log(i)

    i += 1

# 0
# 1
# 2

for i = 0;;
    if i == 3
        break
    
    log(i)

    i += 1

# 0
# 1
# 2

i = 0

for ; i != 3;
    log(i)

    i += 1

# 0
# 1
# 2

i = 0

for ;; i += 1
    if i == 3
        break
    
    log(i)

# 0
# 1
# 2

for int i = 0; i < 3; i += 1
    log(i)

# 0
# 1
# 2

for int i = 0, string a; i < 3; i += 1
    a += i

    log(a)

# 0
# 01
# 012

for int i = 0, string a, a += i; a.length <= 3; i += 1, a += i
    log(a)

# 0
# 01
# 012

for int i = 0, string a = (string)i; a.length <= 3; i += 1, a += i
    log(a)

# 0
# 01
# 012
