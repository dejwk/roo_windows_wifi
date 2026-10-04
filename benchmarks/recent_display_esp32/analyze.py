"""Validate and summarize serial logs or the saved CSV result directory.

Usage: python3 analyze.py RESULT_DIRECTORY
Timing uses uninstrumented passes only. The initial revision predates a known
visibility fix; its rounded-mask drawing is explicitly excluded from equivalence.
"""
import csv, pathlib, statistics, collections, sys
root=pathlib.Path(sys.argv[1]).resolve()
revs=['c243c4f','dbdea8e','c61724a','6b348f3','e745df9','b985f11']
synthetic=[]; wifi=[]; memory=[]; verify=[]; heaps=[]
for rev in revs:
    for suite in ['synthetic','wifi']:
        path=root/f'{rev}_{suite}.log'
        if not path.exists(): continue
        for row in csv.reader(path.read_text(errors='replace').splitlines()):
            if not row: continue
            if row[0]=='DATA': (synthetic if suite=='synthetic' else wifi).append(row[1:])
            elif row[0]=='MEM': memory.append(row[1:])
            elif row[0]=='VERIFY': verify.append(row[1:])
            elif row[0]=='HEAP': heaps.append(row[1:])
headers={
'synthetic':['revision','heap','scene','layers','pixels','mode','phase','samples','mean_us','p50_us','p95_us','p99_us','max_us','allocations','retained_bytes','free_heap','largest_block','checksum'],
'wifi':['revision','pass','repetition','phase','step','cpu_us','allocations','allocated_bytes','frees','pixels','windows','checksum'],
'memory':['revision','heap','scene','layers','pixels','allocations','requested_bytes','peak_live_bytes'],
'verify':['revision','heap','scene','layers','pixels','variant','emitted_pixels','windows','checksum'],
'heaps':['revision','stage','free_heap','largest_block','integrity']}
if not synthetic and (root/'synthetic.csv').exists():
    for name in headers:
        with (root/f'{name}.csv').open() as data:
            rows = list(csv.reader(data))[1:]
        locals()[name].extend(rows)

for name in headers:
    with (root/f'{name}.csv').open('w') as f:
        writer=csv.writer(f, lineterminator="\n");writer.writerow(headers[name]);writer.writerows(locals()[name])
synth={tuple(r[:7]):r for r in synthetic}
print('SYNTHETIC clean heap, microseconds, checkpoints '+','.join(revs))
for scene,n in [('split',4),('overlap',16),('opaque_top',16),('uniform',4),('rounded',4),('opaque_mask',4),('rounded_mask',4)]:
    for area in [768,19200]:
        for phase in ['prepare_destroy','prepare_consume_destroy','draw']:
            values=[synth.get((rev,'clean',scene,str(n),str(area),'fresh',phase)) for rev in revs]
            print(scene,n,area,phase,','.join(r[9] if r else '-' for r in values))
print('WIFI median microseconds; repetitions 2..6')
phases=['editor_to_security','open_security','menu_down','menu_up','close_security','editor_down','editor_up']
for phase in phases:
    values=[[int(r[5]) for r in wifi if r[0]==rev and r[1]=='timing' and 2<=int(r[2])<=6 and r[3]==phase] for rev in revs]
    print(phase,','.join(f'{statistics.median(v):.1f}' if v else '-' for v in values))
for rev in revs:
    sums=collections.defaultdict(int)
    for r in wifi:
        if r[0]==rev and r[1]=='timing' and 2<=int(r[2])<=6: sums[r[2]]+=int(r[5])
    traced=[r for r in wifi if r[0]==rev and r[1]=='trace' and r[2]=='7']
    print('TOTAL',rev,'us',statistics.median(sums.values()) if sums else '-', 'alloc',sum(int(r[6]) for r in traced),'bytes',sum(int(r[7]) for r in traced))
checks=collections.defaultdict(dict)
for r in synthetic:
    if r[6]=='prepare_consume_destroy': checks[('stream',*r[1:6])][r[0]]=r[-1]
for r in verify: checks[('draw',*r[1:6])][r[0]]=(r[6], r[8])
for r in wifi:
    if r[1]=='trace': checks[('wifi',*r[2:5])][r[0]]=(r[9], r[11])
mismatches=[(k,v) for k,v in checks.items() if len(set(v.values()))>1]
print('VERIFICATION',len(checks),'keys;',len(mismatches),'mismatches')

expected_counts = {'synthetic': 1320, 'wifi': 5640, 'memory': 264,
                   'verify': 3168, 'heaps': 30}
for name, expected in expected_counts.items():
    if len(locals()[name]) != expected:
        raise RuntimeError(f'Incomplete {name}: {len(locals()[name])}/{expected}')
if any(len(versions) != 6 for versions in checks.values()):
    raise RuntimeError('Missing revision in pixel comparison')
# 852a8e7 corrected visible drawing of alpha-zero colored samples. Only the
# initial rounded-mask drawing case is intentionally not output-equivalent.
expected_visibility_differences = []
unexpected = []
for key, versions in mismatches:
    later = {rev: value for rev, value in versions.items() if rev != 'c243c4f'}
    if key[0] == 'draw' and key[2] == 'rounded_mask' and len(set(later.values())) == 1:
        expected_visibility_differences.append(key)
    else:
        unexpected.append((key, versions))
print('Known old visibility differences:', len(expected_visibility_differences))
if len(expected_visibility_differences) != 40:
    raise RuntimeError('Historical visibility difference count changed')
if unexpected:
    raise RuntimeError(f'Unexpected pixel differences: {unexpected[:10]}')
if any(row[-1] != '1' for row in heaps):
    raise RuntimeError('Heap integrity failure')
for rev in revs:
    warm = next(r[2:4] for r in heaps if r[:2] == [rev, 'warm'])
    final = next(r[2:4] for r in heaps if r[:2] == [rev, 'after_repeats'])
    if warm != final:
        raise RuntimeError(f'Heap changed across repetitions: {rev}')

print('PASS: complete data, matching pixels except the documented old visibility fix, and stable UI heaps')
