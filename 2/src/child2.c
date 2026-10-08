#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    char *line = NULL;
    size_t capacity = 0;
    int status = EXIT_SUCCESS;

    fprintf(stderr, "[child2 pid=%ld] started\n", (long) getpid());

    for (;;) {
        errno = 0;
        ssize_t length = getline(&line, &capacity, stdin);
        if (length == -1) {
            if (feof(stdin)) {
                break;
            }
            perror("child2: getline");
            status = EXIT_FAILURE;
            break;
        }

        bool previous_was_space = false;
        size_t output_length = 0;
        for (ssize_t i = 0; i < length; ++i) {
            if (line[i] == ' ') {
                if (previous_was_space) {
                    continue;
                }
                previous_was_space = true;
            } else {
                previous_was_space = false;
            }

            line[output_length++] = line[i];
        }
        if (write_all(STDOUT_FILENO, line, output_length) == -1) {
            perror("child2: write");
            status = EXIT_FAILURE;
            break;
        }
    }

    free(line);
    if (fclose(stdin) == EOF) {
        perror("child2: fclose stdin");
        status = EXIT_FAILURE;
    }
    if (close_checked(STDOUT_FILENO, "child2") == -1) {
        status = EXIT_FAILURE;
    }

    fprintf(stderr, "[child2 pid=%ld] finished\n", (long) getpid());
    return status;
}
