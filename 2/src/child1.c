#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    char *line = NULL;
    size_t capacity = 0;
    int status = EXIT_SUCCESS;

    fprintf(stderr, "[child1 pid=%ld] started\n", (long) getpid());

    for (;;) {
        errno = 0;
        ssize_t length = getline(&line, &capacity, stdin);
        if (length == -1) {
            if (feof(stdin)) {
                break;
            }
            perror("child1: getline");
            status = EXIT_FAILURE;
            break;
        }

        for (ssize_t i = 0; i < length; ++i) {
            line[i] = (char) toupper((unsigned char) line[i]);
        }

        if (write_all(STDOUT_FILENO, line, (size_t) length) == -1) {
            perror("child1: write");
            status = EXIT_FAILURE;
            break;
        }
    }

    free(line);
    if (fclose(stdin) == EOF) {
        perror("child1: fclose stdin");
        status = EXIT_FAILURE;
    }
    if (close_checked(STDOUT_FILENO, "child1") == -1) {
        status = EXIT_FAILURE;
    }

    fprintf(stderr, "[child1 pid=%ld] finished\n", (long) getpid());
    return status;
}

