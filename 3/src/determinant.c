#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_N 14
#define MAX_THREADS 256

typedef struct {
    size_t n;
    long double values[MAX_N * MAX_N];
} Matrix;

typedef struct {
    Matrix matrix;
    long double factor;
} Task;

typedef struct {
    Task *tasks;
    size_t count;
    size_t capacity;
    size_t next;
    unsigned hold_seconds;
    pthread_mutex_t mutex;
} Pool;

typedef struct {
    Pool *pool;
    long double result;
} Worker;

static void fail(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static void check_thread(int error, const char *operation)
{
    if (error != 0) {
        fprintf(stderr, "%s: %s\n", operation, strerror(error));
        exit(EXIT_FAILURE);
    }
}

static unsigned parse_number(const char *text, unsigned low, unsigned high)
{
    char *end;
    errno = 0;
    if (text[0] < '0' || text[0] > '9')
        fail("Expected a non-negative integer.");
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || value < low || value > high)
        fail("Integer argument is outside the allowed range.");
    return (unsigned)value;
}

static double now(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double)time.tv_sec + (double)time.tv_nsec / 1e9;
}

static Matrix read_matrix(FILE *input)
{
    Matrix matrix = {0};
    char token[128];
    if (fscanf(input, "%127s", token) != 1)
        fail("Expected the matrix dimension.");
    matrix.n = parse_number(token, 1, MAX_N);
    for (size_t i = 0; i < matrix.n * matrix.n; ++i) {
        if (fscanf(input, "%127s", token) != 1)
            fail("Not enough matrix elements.");
        char *end;
        errno = 0;
        matrix.values[i] = strtold(token, &end);
        if (errno != 0 || end == token || *end != '\0' ||
            !isfinite(matrix.values[i]))
            fail("Expected a finite matrix element.");
    }
    if (fscanf(input, "%127s", token) == 1)
        fail("Unexpected data after the matrix.");
    if (ferror(input))
        fail("Cannot read the matrix.");
    return matrix;
}

static void make_minor(const Matrix *matrix, size_t column, Matrix *minor)
{
    minor->n = matrix->n - 1;
    size_t index = 0;
    for (size_t row = 1; row < matrix->n; ++row)
        for (size_t col = 0; col < matrix->n; ++col)
            if (col != column)
                minor->values[index++] = matrix->values[row * matrix->n + col];
}

static long double determinant(const Matrix *matrix)
{
    if (matrix->n == 1)
        return matrix->values[0];
    long double result = 0;
    Matrix minor;
    for (size_t column = 0; column < matrix->n; ++column) {
        make_minor(matrix, column, &minor);
        long double term = matrix->values[column] * determinant(&minor);
        result += column % 2 == 0 ? term : -term;
    }
    return result;
}

static void prepare_tasks(Pool *pool, const Matrix *matrix,
                          unsigned depth, long double factor)
{
    if (depth == 0 || matrix->n == 1) {
        if (pool->count == pool->capacity) {
            size_t capacity = pool->capacity == 0 ? 16 : pool->capacity * 2;
            Task *tasks = realloc(pool->tasks, capacity * sizeof(*tasks));
            if (tasks == NULL)
                fail("Cannot allocate tasks.");
            pool->tasks = tasks;
            pool->capacity = capacity;
        }
        Task *task = &pool->tasks[pool->count++];
        task->matrix.n = matrix->n;
        memcpy(task->matrix.values, matrix->values,
               matrix->n * matrix->n * sizeof(matrix->values[0]));
        task->factor = factor;
        return;
    }
    Matrix minor;
    for (size_t column = 0; column < matrix->n; ++column) {
        make_minor(matrix, column, &minor);
        long double coefficient = matrix->values[column];
        if (column % 2 != 0)
            coefficient = -coefficient;
        prepare_tasks(pool, &minor, depth - 1, factor * coefficient);
    }
}

static bool take_task(Pool *pool, size_t *index)
{
    check_thread(pthread_mutex_lock(&pool->mutex), "pthread_mutex_lock");
    bool found = pool->next < pool->count;
    if (found)
        *index = pool->next++;
    check_thread(pthread_mutex_unlock(&pool->mutex), "pthread_mutex_unlock");
    return found;
}

static void *worker(void *argument)
{
    Worker *data = argument;
    Pool *pool = data->pool;
    struct timespec remaining = {.tv_sec = pool->hold_seconds, .tv_nsec = 0};
    while (pool->hold_seconds != 0 && nanosleep(&remaining, &remaining) != 0) {
        if (errno != EINTR) {
            perror("nanosleep");
            exit(EXIT_FAILURE);
        }
    }
    size_t index;
    while (take_task(pool, &index)) {
        const Task *task = &pool->tasks[index];
        data->result += task->factor * determinant(&task->matrix);
    }
    return NULL;
}

static void usage(const char *program)
{
    printf("Usage: %s [-t threads] [-d depth] [-w seconds] [matrix.txt]\n"
           "Input: n followed by n*n finite numbers; stdin by default.\n"
           "  -t 1..256  Maximum total threads, including main (default: 1).\n"
           "  -d 0..3    Number of expansion levels used to prepare tasks (default: 2).\n"
           "  -w 0..60   Pause each worker before computation for OS observation.\n"
           "             This pause is included in seconds; use 0 for benchmarks.\n"
           "Dimension: 1..14. Recursive computation has factorial complexity.\n",
           program);
}

int main(int argc, char **argv)
{
    unsigned limit = 1, depth = 2, hold_seconds = 0;
    int option;
    while ((option = getopt(argc, argv, "t:d:w:h")) != -1) {
        switch (option) {
        case 't': limit = parse_number(optarg, 1, MAX_THREADS); break;
        case 'd': depth = parse_number(optarg, 0, 3); break;
        case 'w': hold_seconds = parse_number(optarg, 0, 60); break;
        case 'h': usage(argv[0]); return EXIT_SUCCESS;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }
    if (argc - optind > 1)
        fail("Expected at most one input file.");
    FILE *input = stdin;
    if (optind < argc) {
        input = fopen(argv[optind], "r");
        if (input == NULL) {
            perror(argv[optind]);
            return EXIT_FAILURE;
        }
    }
    Matrix matrix = read_matrix(input);
    if (input != stdin && fclose(input) != 0) {
        perror("fclose");
        return EXIT_FAILURE;
    }

    double started = now();
    Pool pool = {.hold_seconds = hold_seconds};
    prepare_tasks(&pool, &matrix, depth, 1);
    size_t count = limit < pool.count ? limit : pool.count;
    Worker *workers = calloc(count, sizeof(*workers));
    pthread_t *threads = calloc(count, sizeof(*threads));
    if (workers == NULL || threads == NULL)
        fail("Cannot allocate worker data.");
    check_thread(pthread_mutex_init(&pool.mutex, NULL), "pthread_mutex_init");
    for (size_t i = 0; i < count; ++i)
        workers[i].pool = &pool;

    size_t created = 0;
    int create_error = 0;
    for (size_t i = 1; i < count; ++i) {
        create_error = pthread_create(&threads[i], NULL, worker, &workers[i]);
        if (create_error != 0)
            break;
        ++created;
    }
    if (hold_seconds != 0)
        fprintf(stderr, "pid=%ld threads=%zu hold_seconds=%u\n",
                (long)getpid(), created + 1, hold_seconds);
    worker(&workers[0]);
    for (size_t i = 1; i <= created; ++i)
        check_thread(pthread_join(threads[i], NULL), "pthread_join");
    long double result = 0;
    for (size_t i = 0; i <= created; ++i)
        result += workers[i].result;
    double elapsed = now() - started;
    check_thread(pthread_mutex_destroy(&pool.mutex), "pthread_mutex_destroy");
    free(threads);
    free(workers);
    free(pool.tasks);
    check_thread(create_error, "pthread_create");
    if (!isfinite(result))
        fail("The result is not finite: numerical overflow occurred.");
    printf("n=%zu\nthread_limit=%u\nthreads=%zu\ntasks=%zu\ndepth=%u\n"
           "hold_seconds=%u\ndeterminant=%.18Lg\nseconds=%.9f\n",
           matrix.n, limit, count, pool.count, depth, hold_seconds, result, elapsed);
    return EXIT_SUCCESS;
}
