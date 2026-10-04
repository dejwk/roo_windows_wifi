"""Validate and summarize saved counter samples, or extract them from run logs.

Usage: python3 analyze.py RESULTS_DIRECTORY [RUN_DIRECTORY [BASELINE_DIRECTORY]]
When RUN_DIRECTORY is supplied, extract CSVs and provenance into RESULTS_DIRECTORY.
"""
import csv
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import sys

destination = Path(sys.argv[1])
variants = ['runtime_gate', 'hot0000', 'hot0016', 'hot0064', 'hot0256', 'hotnorm']
fields = ['variant', 'round', 'mask_checks', 'stall_mask', 'sample',
          'elapsed_us', 'instructions', 'stall_cycles']
verify_fields = ['variant', 'round', 'clip', 'pixels', 'windows', 'checksum']


def write_csv(name, header, rows):
    with (destination/name).open('w', newline='') as output:
        writer = csv.writer(output, lineterminator='\n')
        writer.writerow(header)
        writer.writerows(rows)


if len(sys.argv) > 2:
    root = Path(sys.argv[2])
    destination.mkdir(parents=True, exist_ok=True)
    samples = []
    checks = []
    provenance = {}
    nm = Path.home()/'.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp-elf-nm'
    for variant in variants:
        path = root/f'{variant}.log'
        lines = path.read_text().splitlines()
        assert 'END,regression' in lines, variant
        for line in lines:
            row = line.split(',')
            if row[0] == 'GATE':
                round_index, enabled, mask = row[1:]
            elif row[0] == 'PERF':
                assert row[1] == variant and row[2] == 'rounded'
                samples.append([variant, round_index, enabled, mask, *row[3:]])
            elif row[0] == 'VERIFY':
                checks.append([variant, round_index, *row[6:]])
        archive = root/'firmware'/variant
        symbols = subprocess.check_output([str(nm), '-C', '-S',
                                            str(archive/'firmware.elf')], text=True)
        relevant = [line for line in symbols.splitlines() if
                    'RasterizableStack::readColorRect(' in line or
                    'DetermineRectColorForRoundRectImpl(' in line]
        provenance[variant] = {
            'symbols': relevant,
            'sha256': {name: hashlib.sha256((archive/name).read_bytes()).hexdigest()
                       for name in ['firmware.elf', 'firmware.bin']},
            'log_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        }
    native = root/'firmware/reloc00'
    if native.exists():
        symbols = subprocess.check_output([str(nm), '-C', '-S',
                                            str(native/'firmware.elf')], text=True)
        provenance['reloc00'] = {
            'symbols': [line for line in symbols.splitlines() if
                        'RasterizableStack::readColorRect(' in line or
                        'DetermineRectColorForRoundRectImpl(' in line],
            'sha256': {name: hashlib.sha256((native/name).read_bytes()).hexdigest()
                       for name in ['firmware.elf', 'firmware.bin']},
            'log_sha256': {
                suite: hashlib.sha256((root/f'reloc00_{suite}.log').read_bytes()).hexdigest()
                for suite in ['synthetic', 'wifi']},
        }
    write_csv('samples.csv', fields, samples)
    write_csv('verify.csv', verify_fields, checks)
    (destination/'firmware.json').write_text(json.dumps(provenance, indent=2)+'\n')

with (destination/'samples.csv').open() as source:
    samples = list(csv.DictReader(source))
with (destination/'verify.csv').open() as source:
    checks = list(csv.DictReader(source))
assert len(samples) == len(variants)*8*48
assert len(checks) == len(variants)*8*12
reference = {}
for check in checks:
    key = check['clip']
    value = tuple(check[name] for name in ['pixels', 'windows', 'checksum'])
    assert reference.setdefault(key, value) == value, check
print(f'Validated {len(samples)} samples and {len(checks)} matching output checks.')
for variant in variants:
    for round_index in range(8):
        group = [row for row in samples if row['variant'] == variant and
                 int(row['round']) == round_index]
        assert len(group) == 48 and {int(row['sample']) for row in group} == set(range(48))
        enabled = str(int(round_index % 2 == 0))
        mask = str([0x1ff, 0x3f, 0x40, 0x180][round_index//2])
        assert all(row['mask_checks'] == enabled and row['stall_mask'] == mask
                   for row in group)
        medians = [statistics.median(int(row[name]) for row in group)
                   for name in ['elapsed_us', 'instructions', 'stall_cycles']]
        print(variant, 'on' if enabled == '1' else 'off', 'mask='+mask, *medians)

# Validate the uninstrumented suites against the saved original b985f11 run.
baseline = (Path(sys.argv[3]) if len(sys.argv) > 3 else
            Path(__file__).resolve().parent.parent/'results/2026-10-04')
names = ['synthetic', 'wifi', 'verify', 'memory', 'heaps']
original = {}
relocated = {}
if len(sys.argv) > 2:
    raw = {name: [] for name in names}
    for suite in ['synthetic', 'wifi']:
        lines = (root/f'reloc00_{suite}.log').read_text().splitlines()
        assert any(line.startswith('END,') for line in lines), suite
        for row in csv.reader(lines):
            name = {'DATA': suite, 'VERIFY': 'verify', 'MEM': 'memory',
                    'HEAP': 'heaps'}.get(row[0] if row else '')
            if name is not None:
                raw[name].append(row[1:])
    for name in names:
        with (baseline/f'{name}.csv').open() as source:
            header = next(csv.reader(source))
        write_csv(f'relocated_{name}.csv', header, raw[name])
for name, count in zip(names, [220, 940, 528, 44, 5]):
    with (baseline/f'{name}.csv').open() as source:
        original[name] = [r for r in csv.DictReader(source) if r['revision'] == 'b985f11']
    with (destination/f'relocated_{name}.csv').open() as source:
        relocated[name] = list(csv.DictReader(source))
    assert len(original[name]) == len(relocated[name]) == count, name
for name in ['verify', 'memory']:
    for before, after in zip(original[name], relocated[name]):
        assert all(before[key] == after[key] for key in before
                   if key != 'revision'), (name, before, after)
for before, after in zip(original['synthetic'], relocated['synthetic']):
    assert all(before[key] == after[key] for key in
               ['heap', 'scene', 'layers', 'pixels', 'mode', 'phase', 'checksum'])
for before, after in zip(original['wifi'], relocated['wifi']):
    assert all(before[key] == after[key] for key in ['pass', 'repetition', 'phase', 'step'])
    if before['pass'] == 'trace':
        assert all(before[key] == after[key] for key in
                   ['allocations', 'allocated_bytes', 'frees', 'pixels', 'windows', 'checksum'])
assert all(row['integrity'] == '1' for row in relocated['heaps'])
warm = next(row for row in relocated['heaps'] if row['stage'] == 'warm')
final = next(row for row in relocated['heaps'] if row['stage'] == 'after_repeats')
assert all(warm[key] == final[key] for key in ['free_heap', 'largest_block'])
print('Uninstrumented suites match: 44 streams, 528 draws, 316 traced UI frames, '
      'allocation traces, and stable UI heaps.')
for label, data in [('original', original), ('relocated', relocated)]:
    for row in data['synthetic']:
        if (row['heap'], row['scene'], row['pixels'], row['phase']) == (
                'clean', 'rounded', '19200', 'draw'):
            print(label, 'rounded p50 us', row['p50_us'])
    sums = defaultdict(int)
    for row in data['wifi']:
        if row['pass'] == 'timing' and 2 <= int(row['repetition']) <= 6:
            sums[row['repetition']] += int(row['cpu_us'])
    print(label, 'median journey us', statistics.median(sums.values()))
