import "env"
    void log(string n)
    
int fd_write(int fd, int iovs, int iovs_len, int nwritten)
    return 0

int fd_read(int fd, int iovs, int iovs_len, int nread)
    return 0

int fd_close(int fd)
    return 0

int args_sizes_get(int argc, int argv_buf_size)
    return 0

int args_get(int argv, int argv_buf)
    return 0

int STDIN = 0
int STDOUT = 1
int STDERR = 2

int ERR_SUCCESS = 0
int ERR_AGAIN = 6

char[][] args()
    int argc = alloc(4)
    int argv_buf_size = alloc(4)

    if args_sizes_get(argc, argv_buf_size) != ERR_SUCCESS
        allocReset()
        return []

    int argv_buf = alloc(readInt(argv_buf_size))
    int argv = alloc(4 * readInt(argc))

    if args_get(argv, argv_buf) != ERR_SUCCESS
        allocReset()
        return []

    char[][] args
    args.reserve(readInt(argc), 0)

    for int i = 0; i < args.length; i += 1
        int ptr = readInt(argv + i * 4)

        char[] arg
        while true
            char c = readChar(ptr)
            if c == '\0'
                break

            arg.push(c)
            ptr += 1

        args[i] = arg
    
    allocReset()
    return args


int close(int fd)
    return -fd_close(fd)

int write(int fd, char[] data)
    int size = data.length
    int base = alloc(size)
    int nwritten = alloc(4)

    for char c in data
        writeChar(base + it, c)

    int iovs = alloc(4 + 4)
    writeInt(iovs, base)
    writeInt(iovs + 4, size)

    int error = fd_write(fd, iovs, 1, nwritten)
    if error != ERR_SUCCESS
        allocReset()
        return -error

    int bytesWritten = readInt(nwritten)

    allocReset()
    return bytesWritten

int read(int fd, char[] buffer)
    int base = alloc(buffer.length)
    int iovs = alloc(4 + 4)
    int nread = alloc(4)
    writeInt(iovs, base)
    writeInt(iovs + 4, buffer.length)

    int err = fd_read(fd, iovs, 1, nread)
    if err != ERR_SUCCESS
        allocReset()
        return -err

    int bytesRead = readInt(nread)
    for int i = 0; i < bytesRead; i += 1
        buffer[i] = readChar(base + i)

    allocReset()
    return bytesRead