import itertools
import math
import random
import subprocess
import sys
from fractions import Fraction

program = sys.argv[1]

def oracle(a):
    n = len(a)
    total = Fraction(0)
    for perm in itertools.permutations(range(n)):
        inversions = sum(perm[i] > perm[j] for i in range(n) for j in range(i + 1, n))
        term = Fraction((-1) ** inversions)
        for row, col in enumerate(perm):
            term *= Fraction(a[row][col])
        total += term
    return total

def encode(a):
    return str(len(a)) + '\n' + '\n'.join(' '.join(map(str, row)) for row in a) + '\n'

rng = random.Random(9)
cases = [
    [[-7]], [[0]], [[2, 3], [4, 5]], [[0, 1], [2, 3]],
    [[1, 2, 3], [1, 2, 3], [4, 5, 6]],
    [[6, 1, 1], [4, -2, 5], [2, 8, 7]],
    [[0.5, 1.25], [2, -0.5]],
    [[int(i == j) for j in range(5)] for i in range(5)],
    [[1 if j >= i else 0 for j in range(5)] for i in range(5)],
    [[0] * 4 for _ in range(4)],
]
for n in range(2, 8):
    for _ in range(2):
        cases.append([[rng.randint(-3, 3) for _ in range(n)] for _ in range(n)])
passed = 0
for a in cases:
    expected = float(oracle(a))
    for depth in range(4):
        task_count = math.prod(range(len(a) - min(depth, len(a) - 1) + 1, len(a) + 1))
        for limit in (1, 2, 4, 8):
            result = subprocess.run([program, '-t', str(limit), '-d', str(depth)],
                                    input=encode(a), text=True, capture_output=True, timeout=30)
            assert result.returncode == 0, result.stderr
            fields = dict(line.split('=', 1) for line in result.stdout.splitlines())
            actual = float(fields['determinant'])
            assert math.isclose(actual, expected, rel_tol=1e-11, abs_tol=1e-10), (a, actual, expected)
            assert int(fields['tasks']) == task_count, fields
            assert int(fields['threads']) == min(limit, task_count), fields
            assert float(fields['seconds']) >= 0, fields
            passed += 1
invalid = [
    ([], ''), ([], '0\n'), ([], '15\n'), ([], '-1\n'),
    ([], '2\n1 2 3\n'), ([], '1\n1 2\n'),
    ([], '1\nnan\n'), ([], '1\ninf\n'), ([], '1\nabc\n'),
    ([], '1\n1e99999\n'), (['-t', '0'], '1\n1\n'),
    (['-t', '-1'], '1\n1\n'), (['-t', '257'], '1\n1\n'),
    (['-t', '2x'], '1\n1\n'), (['-d', '4'], '1\n1\n'),
    (['-w', '61'], '1\n1\n'), (['/missing-lab3-input-file'], ''),
    (['-t'], '1\n1\n'), (['one', 'two'], ''),
]
for args, data in invalid:
    result = subprocess.run([program, *args], input=data, text=True, capture_output=True, timeout=5)
    assert result.returncode != 0, (args, data)
assert subprocess.run([program, '-h'], capture_output=True).returncode == 0
print(f'PASS: {passed} results against exact permutation oracle; {len(invalid)} invalid inputs; help')
