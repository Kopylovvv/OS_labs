#!/bin/sh
# Run from any directory inside a Linux environment with CMake, C compiler, strace.
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$task_root"
if [ "$(uname -s)" != Linux ]; then
    echo 'Run this script in Linux (Ubuntu). macOS cannot provide this strace.' >&2
    exit 1
fi
for tool in cmake strace; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "Missing $tool. In Ubuntu: sudo apt install build-essential cmake strace" >&2
        exit 1
    }
done
mkdir -p report/evidence
{
    uname -a
    cat /etc/os-release
    cc --version
    cmake --version
    strace --version
} > report/evidence/environment.txt 2>&1
cmake -S src -B build-linux -DCMAKE_BUILD_TYPE=Debug > report/evidence/build.txt 2>&1
cmake --build build-linux --parallel >> report/evidence/build.txt 2>&1
ctest --test-dir build-linux --output-on-failure > report/evidence/tests.txt 2>&1
printf 'hello   world\n  a    b  \n' > report/evidence/input.txt
strace -f -s 256 -o report/evidence/strace.txt \
    ./build-linux/parent ./build-linux/child1 ./build-linux/child2 \
    < report/evidence/input.txt \
    > report/evidence/output.txt 2> report/evidence/processes.txt
printf 'HELLO WORLD\n A B \n' > report/evidence/expected.txt
cmp report/evidence/expected.txt report/evidence/output.txt
{
    echo '$ strace -f -s 256 -o report/evidence/strace.txt ./build-linux/parent ./build-linux/child1 ./build-linux/child2 < report/evidence/input.txt'
    echo 'Input:'
    cat report/evidence/input.txt
    echo 'Output:'
    cat report/evidence/output.txt
    echo 'Process diagnostics:'
    cat report/evidence/processes.txt
    echo 'Tests:'
    cat report/evidence/tests.txt
} > report/evidence/demo.txt
# Saving an extract does not alter the complete trace used in report appendix B.
grep -E 'pipe2?\(|clone3?\(|fork\(|dup[23]\(|execve\(|read\(|write\(|close\(|wait4\(|poll\(|ppoll\(|fcntl\(' \
    report/evidence/strace.txt > report/evidence/strace-extract.txt || true
echo 'Linux build, tests and real full strace saved in report/evidence/.'
