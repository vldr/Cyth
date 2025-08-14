import "env"
    void log<T>(T n)

int i = 0
for ;;
    if i == 3
        break
    
    log<int>(i)

    i += 1

# 0
# 1
# 2

for i = 0;;
    if i == 3
        break
    
    log<int>(i)

    i += 1

# 0
# 1
# 2

i = 0

for ; i != 3;
    log<int>(i)

    i += 1

# 0
# 1
# 2

i = 0

for ;; i += 1
    if i == 3
        break
    
    log<int>(i)

# 0
# 1
# 2

for int i = 0; i < 3; i += 1
    log<int>(i)

# 0
# 1
# 2

for int i = 0, string a; i < 3; i += 1
    a += i

    log<string>(a)

# 0
# 01
# 012

for int i = 0, string a, a += i; a.length <= 3; i += 1, a += i
    log<string>(a)

# 0
# 01
# 012

for int i = 0, string a = (string)i; a.length <= 3; i += 1, a += i
    log<string>(a)

# 0
# 01
# 012
