import argparse
import csv
import json
import random
import statistics
import subprocess
from fractions import Fraction
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('program')
parser.add_argument('--docker', help='Run the program in this already-running container')
parser.add_argument('--output', default='/tmp/lab3-benchmark')
parser.add_argument('--repeats', type=int, default=5)
args = parser.parse_args()
if args.repeats < 1:
    parser.error('--repeats must be positive')
output = Path(args.output)
output.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parents[2]
prefix = ['docker', 'exec', '-i', args.docker] if args.docker else []

def exact_det(a):
    a = [[Fraction(x) for x in row] for row in a]
    answer = Fraction(1)
    for col in range(len(a)):
        pivot = next((r for r in range(col, len(a)) if a[r][col]), None)
        if pivot is None:
            return 0
        if pivot != col:
            a[col], a[pivot] = a[pivot], a[col]
            answer = -answer
        value = a[col][col]
        answer *= value
        for row in range(col + 1, len(a)):
            factor = a[row][col] / value
            for j in range(col + 1, len(a)):
                a[row][j] -= factor * a[col][j]
    assert answer.denominator == 1
    return answer.numerator

raw = []
summary = []
for n in (5, 8, 10, 11):
    rng = random.Random(9000 + n)
    a = [[rng.choice((-3, -2, -1, 1, 2, 3)) for _ in range(n)] for _ in range(n)]
    text = str(n) + '\n' + '\n'.join(' '.join(map(str, row)) for row in a) + '\n'
    (root / 'src' / 'tests' / 'data' / f'benchmark{n}.txt').write_text(text)
    expected = exact_det(a)
    samples = {p: [] for p in (1, 2, 4, 8)}
    orders = []
    for repeat in range(args.repeats + 1):
        order = list(samples)
        random.Random(90 + repeat).shuffle(order)
        orders.append(order)
        for p in order:
            result = subprocess.run(prefix + [args.program, '-t', str(p), '-d', '2'],
                                    input=text, text=True, capture_output=True, check=True, timeout=120)
            fields = dict(line.split('=', 1) for line in result.stdout.splitlines())
            assert Fraction(fields['determinant']) == expected, (n, p, fields, expected)
            assert int(fields['threads']) == p
            if repeat != 0:
                seconds = float(fields['seconds'])
                samples[p].append(seconds)
                raw.append(dict(n=n, threads=p, repeat=repeat, seconds=seconds, determinant=expected))
    baseline = statistics.median(samples[1])
    for p, values in samples.items():
        median = statistics.median(values)
        row = dict(n=n, threads=p, median_seconds=median, min_seconds=min(values),
                   max_seconds=max(values), speedup=baseline / median,
                   efficiency=baseline / median / p)
        summary.append(row)
        print(row, flush=True)
for name, rows in [('benchmark-raw.csv', raw), ('benchmark-summary.csv', summary)]:
    with (output / name).open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
(output / 'benchmark-method.json').write_text(json.dumps(dict(
    sizes=[5,8,10,11], threads=[1,2,4,8], depth=2, repeats=args.repeats,
    seed='9000+n', entries=[-3,-2,-1,1,2,3], warmups_per_configuration=1,
    timer='CLOCK_MONOTONIC; preparation + allocation + create + compute + join + final sum',
    excluded='input parsing, process startup, output, deallocation; no strace and no -w',
    execution='sequential configurations; deterministic shuffled order per repeat',
    reference='exact rational Gaussian elimination in benchmark driver only'
), indent=2))
