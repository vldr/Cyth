void log(int n)

class Hello
    int data
    Hello next

    int getData()
        return data

    Hello getNext()
        return this.next

    void setData(int data_)
        data = data_

    void setNext(Hello next_)
        next = next_

Hello a = Hello()
Hello b = Hello()
Hello c = Hello()
Hello d = Hello()

a.setData(1)
a.setNext(b)

b.setData(2)
b.setNext(c)

c.setData(3)
c.setNext(d)

d.setData(4)

Hello current = a
while current != null
    log(current.getData())
    current = current.getNext()

# b.setData(2)
# b.setNext(c)

# c.setData(3)
# c.setNext(d)

# d.setData(4)


# void log(int n)

# class Hello
#     int data
#     Hello next

#     int getData()
#         return getNext().getNext().getNext().next.data

#     Hello getNext()
#         return next

# Hello a = Hello()
# Hello b = Hello()
# Hello c = Hello()
# Hello d = Hello()

# a.data = 1
# a.next = b

# b.data = 2
# b.next = c

# c.data = 3
# c.next = d

# d.data = 4

# Hello current = a
# while current != null
#     log(current.getData())
#     current = current.getNext()