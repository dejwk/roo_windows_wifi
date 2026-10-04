import csv
import pathlib
import statistics
import sys
from collections import defaultdict

root = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parent
header = ['variant', 'pass', 'repetition', 'phase', 'step', 'cpu_us', 'allocations', 'allocated_bytes', 'frees', 'pixels', 'windows', 'checksum']
rows = []
for variant in ['baseline', 'fresh', 'reuse']:
    path = root / (variant + '_serial.log')
    lines = path.read_text(errors='replace').splitlines()
    assert any(s.startswith('END,') for s in lines), f'{variant} incomplete'
    for line in lines:
        if not line.startswith('DATA,'): continue
        fields = next(csv.reader([line]))[1:]
        row = dict(zip(header, fields, strict=True))
        for field in header[2:-1]:
            if field != 'phase': row[field] = int(row[field])
        rows.append(row)
with (root/'results.csv').open('w') as f:
    writer = csv.DictWriter(f, header, lineterminator='\n')
    writer.writeheader()
    writer.writerows(rows)

checks = defaultdict(dict)
for row in rows:
    if row['pass'] == 'trace':
        checks[(row['repetition'],row['phase'],row['step'])][row['variant']] = (row['pixels'],row['windows'],row['checksum'])
for key, values in checks.items():
    assert len(values) == 3 and len(set(values.values())) == 1, (key,values)
print(f'All {len(checks)} traced frames match pixels, windows and color/position checksums across all three builds.')

phases = list(dict.fromkeys(row['phase'] for row in rows))
for phase in phases:
    print('\n'+phase)
    for variant in ['baseline','fresh','reuse']:
        timed = [r for r in rows if r['variant']==variant and r['phase']==phase and r['pass']=='timing' and r['repetition']>=2]
        traced = [r for r in rows if r['variant']==variant and r['phase']==phase and r['repetition']==17]
        if not timed: continue
        samples = sorted(r['cpu_us'] for r in timed)
        allocations = [r['allocations'] for r in traced]
        print(f"  {variant:8} n={len(samples)} median_us={statistics.median(samples):.1f} p95_us={samples[int((len(samples)-1)*.95)]} minmax_us={samples[0]}/{samples[-1]} alloc_median={statistics.median(allocations)} alloc_range={min(allocations)}/{max(allocations)} bytes_median={statistics.median(r['allocated_bytes'] for r in traced)}")

print('\nComplete repeated journey (sum of frame CPU times)')
for variant in ['baseline','fresh','reuse']:
    durations=defaultdict(int)
    for r in rows:
        if r['variant']==variant and r['pass']=='timing' and r['repetition']>=2:
            durations[r['repetition']]+=r['cpu_us']
    trace=[r for r in rows if r['variant']==variant and r['repetition']==17]
    print(variant, 'frames',len(trace),'median_us',statistics.median(durations.values()),'allocations',sum(r['allocations'] for r in trace),'bytes',sum(r['allocated_bytes'] for r in trace))

print('\nHeap')
for variant in ['baseline','fresh','reuse']:
    for line in (root/(variant+'_serial.log')).read_text(errors='replace').splitlines():
        if line.startswith('HEAP,'): print(line)
