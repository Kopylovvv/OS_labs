#!/bin/sh
set -eu
program=$1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
printf '3\n6 1 1\n4 -2 5\n2 8 7\n' > "$tmp/matrix.txt"
for threads in 1 2 4 8; do
    for depth in 0 1 2 3; do
        "$program" -t "$threads" -d "$depth" "$tmp/matrix.txt" > "$tmp/result"
        grep -qx 'determinant=-306' "$tmp/result"
    done
done
printf '1\n-7\n' | "$program" -t 256 > "$tmp/result"
grep -qx 'threads=1' "$tmp/result"
grep -qx 'determinant=-7' "$tmp/result"
for bad in '0' '-1' 'abc' '257'; do
    if "$program" -t "$bad" "$tmp/matrix.txt" > /dev/null 2>&1; then
        echo "Accepted invalid thread limit: $bad" >&2
        exit 1
    fi
done
if printf '2\n1 2 3\n' | "$program" > /dev/null 2>&1; then
    echo 'Accepted incomplete matrix' >&2
    exit 1
fi
echo 'PASS: 16 determinant configurations, scalar matrix, 5 invalid inputs'
