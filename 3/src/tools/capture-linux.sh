#!/bin/sh
set -eu
program=${1:-/tmp/lab3-build/determinant}
output=${2:-/tmp/lab3-evidence}
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
mkdir -p "$output"
for threads in 1 2 4 8; do
    strace -f -s 128 -e trace=clone,clone3,futex,clock_nanosleep,exit,exit_group \
        -o "$output/strace-t$threads.txt" \
        "$program" -t "$threads" -w 2 "$root/src/tests/data/matrix5.txt" \
        > "$output/result-t$threads.txt" 2> "$output/ready-t$threads.txt" &
    tracer=$!
    tries=0
    while ! test -s "$output/ready-t$threads.txt"; do
        tries=$((tries + 1))
        if test "$tries" -gt 100; then
            kill "$tracer" 2>/dev/null || true
            echo 'No readiness message from program' >&2
            exit 1
        fi
        sleep 0.02
    done
    pid=$(sed -n 's/^pid=\([0-9]*\).*/\1/p' "$output/ready-t$threads.txt")
    test -n "$pid"
    ps -L -p "$pid" -o pid,tid,nlwp,stat,comm > "$output/ps-t$threads.txt"
    actual=$(ls "/proc/$pid/task" | wc -l)
    test "$actual" -eq "$threads"
    grep '^Threads:' "/proc/$pid/status" > "$output/proc-t$threads.txt"
    wait "$tracer"
    printf 'PASS: requested=%s observed=%s\n' "$threads" "$actual"
done
