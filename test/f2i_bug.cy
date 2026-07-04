void(float) test = apple
test(9000000000.0)

# 1

void apple(float delta)
    int deltaFloat = (int)delta

    log(deltaFloat == -2147483648 or deltaFloat == 2147483647)
