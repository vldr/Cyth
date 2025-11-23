import "env"
    void log(string n) 
    void log(int n)

string reverseWords(string s)
    string output
    int end

    for int start = 0; start < s.length; start = start + 1
        if start == s.length - 1 or s[start] == ' '
            int curr = start
            if s[start] == ' '
                curr = curr - 1

            while curr >= end
                output = output + s[curr]
                curr = curr - 1

            if s[start] == ' '
                output = output + ' '
            
            end = start + 1

    return output

log(reverseWords("Mr Ding"))

# rM gniD