import "env"
    void log(float x)
    void log(int n)

log(0x00_00_1)
log(0x0_1_0)

log(1_2)
log(1_2_3)
log(1_23_4)

log(1_2.6_2_5)
log(1_2_3.62_5)
log(1_23_4.6_25)

# 1
# 16
# 12
# 123
# 1234
# 12.625
# 123.625
# 1234.625