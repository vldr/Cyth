import "env"
  void log(int n)
  void log(string n)

int[][] decodeMappings(string mappings)
  int offset = 0
  int file = 0
  int line = 0
  int column = 0
  int index = 0
  int[][] result

  while index < mappings.length
    int[] decodeVLQ(string mappings, int index)
      int output 
      int shift
      int sign = 1

      while true
        if index >= mappings.length
          break

        char character = mappings[index]
        if character == ','
          break

        int value
        if character >= 'A' and character <= 'Z'
          value = character - 'A'
        else if character >= 'a' and character <= 'z'
          value = 26 + (character - 'a')
        else if character >= '0' and character <= '9'
          value = 52 + (character - '0')
        else if character == '+'
          value = 62
        else if character == '/'
          value = 63

        int continuation = value & 32

        if shift
          int digit = value & 31
          output |= digit << shift
          shift += 5
        else
          if value & 1
            sign = -sign
          
          int digit = (value >> 1) & 15
          output |= digit
          shift += 4

        index += 1

        if not continuation
          break

      return [index, output * sign]
    
    int[] vlq = decodeVLQ(mappings, index)
    index = vlq[0]
    offset += vlq[1]

    vlq = decodeVLQ(mappings, index)
    index = vlq[0]
    file += vlq[1]

    vlq = decodeVLQ(mappings, index)
    index = vlq[0]
    line += vlq[1]

    vlq = decodeVLQ(mappings, index)
    index = vlq[0]
    column += vlq[1]

    decodeVLQ(mappings, index)

    result.push([ offset, line, column ])
    index += 1

  return result


log((string)decodeMappings("AAA,IAM"))

#[\n [0, 0, 0],\n [4, 6, 0]\n]