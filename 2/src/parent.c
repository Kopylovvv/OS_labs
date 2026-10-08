#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum { READ_END = 0, WRITE_END = 1 };

static int close_pipe(int pipe_fd[2], const char *name) {
    int status = 0;
    if (close_checked(pipe_fd[READ_END], name) == -1) {
        status = -1;
    }
    pipe_fd[READ_END] = -1;
    if (close_checked(pipe_fd[WRITE_END], name) == -1) {
        status = -1;
    }
    pipe_fd[WRITE_END] = -1;
    return status;
}

static int close_three_pipes(int first[2], int second[2], int third[2],
                             const char *name) {
    int status = 0;
    if (close_pipe(first, name) == -1) {
        status = -1;
    }
    if (close_pipe(second, name) == -1) {
        status = -1;
    }
    if (close_pipe(third, name) == -1) {
        status = -1;
    }
    return status;
}

static void child1_process(int input_pipe[2], int middle_pipe[2],
                           int output_pipe[2], const char *child1_path) {
    if (dup2_checked(input_pipe[READ_END], STDIN_FILENO, "child1") == -1 ||
        dup2_checked(middle_pipe[WRITE_END], STDOUT_FILENO, "child1") == -1) {
        _exit(126);
    }

    if (close_three_pipes(input_pipe, middle_pipe, output_pipe, "child1") == -1) {
        _exit(126);
    }

    execl(child1_path, child1_path, (char *) NULL);
    perror("parent: exec child1");
    _exit(127);
}

static void child2_process(int input_pipe[2], int middle_pipe[2],
                           int output_pipe[2], const char *child2_path) {
    if (dup2_checked(middle_pipe[READ_END], STDIN_FILENO, "child2") == -1 ||
        dup2_checked(output_pipe[WRITE_END], STDOUT_FILENO, "child2") == -1) {
        _exit(126);
    }

    if (close_three_pipes(input_pipe, middle_pipe, output_pipe, "child2") == -1) {
        _exit(126);
    }

    execl(child2_path, child2_path, (char *) NULL);
    perror("parent: exec child2");
    _exit(127);
}

static int wait_for_child(pid_t pid, const char *name) {
    int child_status;
    pid_t result;

    do {
        result = waitpid(pid, &child_status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        fprintf(stderr, "parent: waitpid %s: ", name);
        perror(NULL);
        return -1;
    }
    if (WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0) {
        return 0;
    }
    if (WIFEXITED(child_status)) {
        fprintf(stderr, "parent: %s exited with code %d\n",
                name, WEXITSTATUS(child_status));
    } else if (WIFSIGNALED(child_status)) {
        fprintf(stderr, "parent: %s terminated by signal %d\n",
                name, WTERMSIG(child_status));
    }
    return -1;
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("parent: fcntl");
        return -1;
    }
    return 0;
}

static int exchange_line(int input_fd, int output_fd, const char *line,
                         size_t length, bool add_newline) {
    size_t sent = 0;
    size_t total = length + (add_newline ? 1u : 0u);
    bool received_newline = false;

    while (sent < total || !received_newline) {
        struct pollfd fds[2] = {
            { .fd = input_fd, .events = sent < total ? POLLOUT : 0 },
            { .fd = output_fd, .events = received_newline ? 0 : POLLIN }
        };

        int ready;
        do {
            ready = poll(fds, 2, -1);
        } while (ready == -1 && errno == EINTR);
        if (ready == -1) {
            perror("parent: poll");
            return -1;
        }

        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            char buffer[4096];
            ssize_t count = read(output_fd, buffer, sizeof(buffer));
            if (count == 0) {
                fprintf(stderr, "parent: result pipe closed unexpectedly\n");
                return -1;
            }
            if (count == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                perror("parent: read result");
                return -1;
            }
            if (count > 0) {
                for (ssize_t i = 0; i < count; ++i) {
                    if (buffer[i] == '\n') {
                        received_newline = true;
                        if (i != count - 1) {
                            fprintf(stderr, "parent: unexpected extra output\n");
                            return -1;
                        }
                    }
                }
                if (write_all(STDOUT_FILENO, buffer, (size_t) count) == -1) {
                    perror("parent: write stdout");
                    return -1;
                }
            }
        }

        if (fds[0].revents & POLLOUT) {
            const char *data = sent < length ? line + sent : "\n";
            size_t remaining = sent < length ? length - sent : 1;
            ssize_t count = write(input_fd, data, remaining);
            if (count > 0) {
                sent += (size_t) count;
            } else if (count == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                perror("parent: write input pipe");
                return -1;
            }
        }

        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "parent: input pipe closed unexpectedly\n");
            return -1;
        }
        if (fds[1].revents & POLLNVAL) {
            fprintf(stderr, "parent: invalid result pipe descriptor\n");
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s PATH_TO_CHILD1 PATH_TO_CHILD2\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("parent: signal");
        return EXIT_FAILURE;
    }

    int input_pipe[2] = {-1, -1};
    int middle_pipe[2] = {-1, -1};
    int output_pipe[2] = {-1, -1};
    pid_t child1_pid = -1;
    pid_t child2_pid = -1;
    int status = EXIT_FAILURE;

    if (pipe(input_pipe) == -1) {
        perror("parent: pipe input");
        goto cleanup;
    }
    if (pipe(middle_pipe) == -1) {
        perror("parent: pipe middle");
        goto cleanup;
    }
    if (pipe(output_pipe) == -1) {
        perror("parent: pipe output");
        goto cleanup;
    }

    child1_pid = fork();
    if (child1_pid == -1) {
        perror("parent: fork child1");
        goto cleanup;
    }
    if (child1_pid == 0) {
        child1_process(input_pipe, middle_pipe, output_pipe, argv[1]);
    }

    child2_pid = fork();
    if (child2_pid == -1) {
        perror("parent: fork child2");
        goto cleanup;
    }
    if (child2_pid == 0) {
        child2_process(input_pipe, middle_pipe, output_pipe, argv[2]);
    }

    int close_status = 0;
    if (close_checked(input_pipe[READ_END], "parent") == -1) {
        close_status = -1;
    }
    input_pipe[READ_END] = -1;
    if (close_checked(middle_pipe[READ_END], "parent") == -1) {
        close_status = -1;
    }
    middle_pipe[READ_END] = -1;
    if (close_checked(middle_pipe[WRITE_END], "parent") == -1) {
        close_status = -1;
    }
    middle_pipe[WRITE_END] = -1;
    if (close_checked(output_pipe[WRITE_END], "parent") == -1) {
        close_status = -1;
    }
    output_pipe[WRITE_END] = -1;
    if (close_status == -1) {
        goto cleanup;
    }
    if (set_nonblocking(input_pipe[WRITE_END]) == -1 ||
        set_nonblocking(output_pipe[READ_END]) == -1) {
        goto cleanup;
    }

    fprintf(stderr, "[parent pid=%ld, child1=%ld, child2=%ld] started\n",
            (long) getpid(), (long) child1_pid, (long) child2_pid);
    fprintf(stderr, "Enter lines. Press Ctrl-D to finish.\n");

    char *line = NULL;
    size_t capacity = 0;
    for (;;) {
        errno = 0;
        ssize_t length = getline(&line, &capacity, stdin);
        if (length == -1) {
            if (feof(stdin)) {
                status = EXIT_SUCCESS;
            } else {
                perror("parent: getline");
            }
            break;
        }

        if (exchange_line(input_pipe[WRITE_END], output_pipe[READ_END],
                          line, (size_t) length, length == 0 || line[length - 1] != '\n') == -1) {
            break;
        }
    }
    free(line);

cleanup:
    if (close_three_pipes(input_pipe, middle_pipe, output_pipe, "parent") == -1) {
        status = EXIT_FAILURE;
    }

    int children_ok = 0;
    if (child1_pid > 0 && wait_for_child(child1_pid, "child1") == -1) {
        children_ok = -1;
    }
    if (child2_pid > 0 && wait_for_child(child2_pid, "child2") == -1) {
        children_ok = -1;
    }
    if (children_ok == -1) {
        status = EXIT_FAILURE;
    }

    fprintf(stderr, "[parent pid=%ld] finished\n", (long) getpid());
    return status;
}
