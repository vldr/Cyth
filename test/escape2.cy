log((int)"\x0"[0])
# 0

log((int)"\x00"[0])
# 0

log((int)"\xf"[0])
# 15

log((int)"\x0f"[0])
# 15

log((int)"\xfg"[0])
# 15

log((int)"\xff"[0])
# 255

log((int)"\xffg"[0])
# 255

log((int)"\xffg"[1])
# 103


