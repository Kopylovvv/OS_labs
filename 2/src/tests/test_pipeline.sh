#!/bin/sh
set -eu

parent=$1
child1=$2
child2=$3
temporary_directory=$(mktemp -d "${TMPDIR:-/tmp}/lab2-test.XXXXXX")
trap 'rm -rf "$temporary_directory"' EXIT HUP INT TERM

run_case() {
    name=$1
    input=$2
    expected=$3

    printf '%s' "$input" | "$parent" "$child1" "$child2" \
        >"$temporary_directory/actual" 2>"$temporary_directory/diagnostics"
    printf '%s' "$expected" >"$temporary_directory/expected"

    if ! cmp -s "$temporary_directory/expected" "$temporary_directory/actual"; then
        echo "FAILED: $name" >&2
        diff -u "$temporary_directory/expected" "$temporary_directory/actual" >&2 || true
        exit 1
    fi
    echo "PASSED: $name"
}

run_case "uppercase and spaces" \
    'Hello   world
' \
    'HELLO WORLD
'

run_case "leading and trailing spaces" \
    '  one    two  
' \
    ' ONE TWO 
'

run_case "empty line" '
' '
'

run_case "no duplicate spaces" \
    'Already_ok 123!
' \
    'ALREADY_OK 123!
'

run_case "last line without newline" \
    'last   line' \
    'LAST LINE
'

long_input=$temporary_directory/long_input
long_expected=$temporary_directory/long_expected
awk 'BEGIN { printf "a"; for (i = 0; i < 100000; ++i) printf " "; print "b" }' >"$long_input"
printf 'A B\n' >"$long_expected"
"$parent" "$child1" "$child2" <"$long_input" \
    >"$temporary_directory/actual" 2>"$temporary_directory/diagnostics"
cmp "$long_expected" "$temporary_directory/actual"
echo "PASSED: long line"

awk 'BEGIN { for (i = 0; i < 100000; ++i) printf "ab"; print "" }' >"$long_input"
awk 'BEGIN { for (i = 0; i < 100000; ++i) printf "AB"; print "" }' >"$long_expected"
"$parent" "$child1" "$child2" <"$long_input" \
    >"$temporary_directory/actual" 2>"$temporary_directory/diagnostics"
cmp "$long_expected" "$temporary_directory/actual"
echo "PASSED: long line without spaces"

if printf 'test\n' | "$parent" /path/that/does/not/exist "$child2" \
    >"$temporary_directory/actual" 2>"$temporary_directory/diagnostics"; then
    echo "FAILED: broken child must cause a non-zero exit status" >&2
    exit 1
fi
echo "PASSED: broken child connection"
