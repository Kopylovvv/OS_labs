#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$task_root"
mkdir -p report/evidence
: > report/evidence/faults.txt
for specification in pipe2:error=EMFILE:when=1 clone:error=EAGAIN:when=1 clone:error=EAGAIN:when=2; do
    label=$(printf '%s' "$specification" | tr ':=' '__')
    if timeout 15 strace -f -e "inject=$specification" \
        -o "report/evidence/fault-$label.trace" \
        ./build-linux/parent ./build-linux/child1 ./build-linux/child2 \
        < report/evidence/input.txt > "report/evidence/fault-$label.out" \
        2> "report/evidence/fault-$label.err"; then
        echo "Expected failure not detected: $specification" >&2
        exit 1
    else
        result=$?
        [ "$result" -eq 1 ] || { echo "Unexpected exit $result: $specification" >&2; exit 1; }
    fi
    {
        echo "inject=$specification; exit=1; no timeout"
        cat "report/evidence/fault-$label.err"
    } >> report/evidence/faults.txt
done
# Allocation failure in getline: 32 MiB virtual-memory limit, larger input stream.
if awk 'BEGIN { for (i=0; i<50000000; ++i) printf "a"; print "" }' 2>/dev/null | \
    (ulimit -v 32768; timeout 15 ./build-linux/parent ./build-linux/child1 ./build-linux/child2) \
    > report/evidence/memory-error.out 2> report/evidence/memory-error.err; then
    echo 'Memory-limit test unexpectedly succeeded' >&2
    exit 1
else
    result=$?
    [ "$result" -eq 1 ] || { echo "Memory-limit test exited $result" >&2; exit 1; }
fi
grep -q 'Cannot allocate memory' report/evidence/memory-error.err
{
    echo 'getline allocation failure: virtual-memory limit 32768 KiB; exit=1; no timeout'
    cat report/evidence/memory-error.err
} >> report/evidence/faults.txt
echo 'Fault-injection and memory-limit tests passed.'
