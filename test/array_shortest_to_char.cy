import "env"
    void log(string n)
    void log(int n)

int[] shortestToChar(string s, char c)
    int[] list

    int ch = -1
    for int i = 0; i < s.length; i += 1
        if s[i] == c
            ch = i

        if ch == -1
            list.push(2147483647)
        else
            list.push(i - ch)

    ch = -1
    for int i = s.length - 1; i >= 0; i = i - 1
        if s[i] == c
            ch = i

        if ch == -1
            continue
        
        int dist = ch - i
        if dist < list[i]
            list[i] = dist

    return list
    
int[] result = shortestToChar("aaaaaaaaaaaab", 'b')

string buffer
for int i = 0; i < result.length; i += 1
    buffer = buffer + result[i] + " "

log(buffer)

# 12 11 10 9 8 7 6 5 4 3 2 1 0 