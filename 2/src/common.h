#ifndef LAB2_COMMON_H
#define LAB2_COMMON_H

#include <stddef.h>
#include <sys/types.h>

int close_checked(int fd, const char *name);
int dup2_checked(int old_fd, int new_fd, const char *name);
ssize_t write_all(int fd, const void *buffer, size_t count);

#endif

