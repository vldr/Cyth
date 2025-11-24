import "env"
    void log(string n)
    void log(int n)

log((string)"apple,banana,cherry".split(","))
# [apple, banana, cherry]

log((string)"one two three".split(" "))
# [one, two, three]

log((string)"2025-11-09".split("-"))
# [2025, 11, 09]

log((string)"hello".split(","))
# [hello]

log((string)"".split(","))
# []

log((string)"apple,,banana".split(","))
# [apple, , banana]

log((string)"---".split("-"))
# [, , , ]


log((string)"a|b|c".split("|"))
# [a, b, c]

log((string)"line1\nline2\nline3".split("\n"))
# [line1, line2, line3]

log((string)"word.word.word".split("."))
# [word, word, word]

log((string)"key=>value=>next".split("=>"))
# [key, value, next]

log((string)"abc<->def<->ghi".split("<->"))
# [abc, def, ghi]

log((string)",start,middle,end,".split(","))
# [, start, middle, end, ]

log((string)"  spaced  ".split(" "))
# [, , spaced, , ]


log((string)"solostring".split(","))
# [solostring]

log((string)"nothing-to-split".split("/"))
# [nothing-to-split]

log((string)"a--b---c".split("-"))
# [a, , b, , , c]

log((string)"xxxyxx".split("x"))
# [, , , y, , ]

log((string)"   ".split(" "))
# [, , , ]

log((string)"tab\tseparated\tvalues".split("\t"))
# [tab, separated, values]


log((string)"café-au-lait".split("-"))
# [café, au, lait]

log((string)"😀😂🤣".split("😂"))
# [😀, 🤣]

log((string)"日本語-漢字-かな".split("-"))
# [日本語, 漢字, かな]

log((string)"alpha<>beta<>gamma".split("<>"))
# [alpha, beta, gamma]

log((string)"foo***bar***baz".split("***"))
# [foo, bar, baz]

log((string)"abc".split(""))
# [a, b, c]

log((string)"a".split(""))
# [a]

log((string)"".split(""))
# []

log((string)"abc".split("abc"))
# [, ]

log((string)",apple,banana".split(","))
# [, apple, banana]

log((string)"-start-middle".split("-"))
# [, start, middle]


