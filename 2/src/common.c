#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int close_checked(int fd, const char *name) {
    if (fd < 0) {
        return 0;
    }

    if (close(fd) == -1) {
        fprintf(stderr, "%s: close: ", name);
        perror(NULL);
        return -1;
    }
    return 0;
}

int dup2_checked(int old_fd, int new_fd, const char *name) {
    if (dup2(old_fd, new_fd) == -1) {
        fprintf(stderr, "%s: dup2: ", name);
        perror(NULL);
        return -1;
    }
    return 0;
}

ssize_t write_all(int fd, const void *buffer, size_t count) {
    const unsigned char *data = buffer;
    size_t written = 0;

    while (written < count) {
        ssize_t result = write(fd, data + written, count - written);
        if (result > 0) {
            written += (size_t) result;
            continue;
        }
        if (result == -1 && errno == EINTR) {
            continue;
        }
        return -1;
    }

    return (ssize_t) written;
}

