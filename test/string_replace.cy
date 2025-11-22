import "env"
    void log(string n)
    void log(int n)

log((string)"applesauce".replace("apple", "orange"))
# orangesauce

log((string)"applesauce".replace("sauce", "juice"))
# applejuice

log((string)"applesauce".replace("a", "A"))
# ApplesAuce

log((string)"applesauce".replace("z", "Z"))
# applesauce

log((string)"".replace("", "-"))
# -

log((string)"".replace("a", "b"))
# 

log((string)"aaaaa".replace("aa", "b"))
# bba

log((string)"banana".replace("na", "NA"))
# baNANA

log((string)"banana".replace("banana", "fruit"))
# fruit

log((string)"banana".replace("banana", ""))
# 

log((string)"banana".replace("ban", ""))
# ana

log((string)"banana".replace("ana", ""))
# bna

log((string)"banana".replace("nana", "x"))
# bax

log((string)"banana".replace("bana", "x"))
# xna

log((string)"banana".replace("banana!", "x"))
# banana

log((string)"😀😃😀😃😀".replace("😀", "😄"))
# 😄😃😄😃😄

log((string)"😀😃😀😃😀".replace("😃", "😆"))
# 😀😆😀😆😀

log((string)"𠮷𠮷𠮷".replace("𠮷", "吉"))
# 吉吉吉

log((string)"👍👍👍".replace("👍", "👎"))
# 👎👎👎

log((string)"straße".replace("ß", "ss"))
# strasse

log((string)"tab\tchar".replace("\t", " "))
# tab char

log((string)"a\nb\nc".replace("\n", ","))
# a,b,c

log((string)"repeat repeat repeat".replace("repeat", "done"))
# done done done

log((string)"repeat repeat repeat".replace("repeat ", ""))
# repeat

log((string)"applesauce".replace("apple", ""))
# sauce

log((string)"applesauce".replace("app", ""))
# lesauce

log((string)"applesauce".replace("le", "LE"))
# appLEsauce

log((string)"applesauce".replace("s", "S"))
# appleSauce

log((string)"applesauce".replace("e", "E"))
# applEsaucE

log((string)"applesauce".replace("applesauce", "jam"))
# jam

log((string)"applesauce".replace("applesauces", "jam"))
# applesauce

log((string)"applesauce".replace("ap", "AP"))
# APplesauce

log((string)"applesauce".replace("ppl", "PPL"))
# aPPLesauce

log((string)"applesauce".replace("au", "AU"))
# applesAUce

log((string)"applesauce".replace("apple", "applepie"))
# applepiesauce

log((string)"applesauce".replace("applepie", "x"))
# applesauce

log((string)"applesauce".replace("a", "aa"))
# aapplesaauce

log((string)"aaaa".replace("aa", "b"))
# bb

log((string)"ababab".replace("ab", "x"))
# xxx

log((string)"ababab".replace("aba", "y"))
# ybab

log((string)"banana".replace("a", ""))
# bnn

log((string)"banana".replace("n", "N"))
# baNaNa

log((string)"banana".replace("na", "NA"))
# baNANA

log((string)"banana".replace("", "_--_"))
# _--_b_--_a_--_n_--_a_--_n_--_a_--_

log((string)"".replace("", "x"))
# x

log((string)"".replace("a", "b"))
# 

log((string)"abc".replace("bc", ""))
# a

log((string)"abc".replace("abcd", "x"))
# abc

log((string)"😀😃😄".replace("😃", "🙂"))
# 😀🙂😄

log((string)"😀😃😀".replace("😀", "😅"))
# 😅😃😅

log((string)"𠮷𠮷".replace("𠮷", "吉"))
# 吉吉

log((string)"straße".replace("ß", "ss"))
# strasse

log((string)"straße".replace("stra", "STRA"))
# STRAße

log((string)"tab\tchar".replace("\t", "_T_"))
# tab_T_char

log((string)"line1\nline2\n".replace("\n", "|"))
# line1|line2|

log((string)"repeat repeat repeat".replace(" repeat", ""))
# repeat

log((string)"edgecase".replace("edgecase", "edgecase"))
# edgecase

log((string)"casesensitive".replace("Case", "case"))
# casesensitive

log((string)"multiple".replace("m", "M"))
# Multiple

log((string)"multiple".replace("M", "m"))
# multiple

log((string)"mixedCASE".replace("CASE", "case"))
# mixedcase

log((string)"startend".replace("start", ""))
# end

log((string)"empty".replace("", ""))
# empty

log((string)"".replace("", ""))
# 

log((string)"".replace("test", ""))
# 

log((string)"".replace("", "test"))
# test

