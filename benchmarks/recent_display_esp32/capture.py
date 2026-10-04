"""Reset ESP32, select S (synthetic) or W (Wi-Fi), and capture one suite.

Usage: <PlatformIO python> capture.py output.log S|W
"""
import pathlib, sys, time, serial
path=pathlib.Path(sys.argv[1])
command=sys.argv[2]
port=serial.Serial(port=None,baudrate=115200,timeout=1)
port.dtr=False
port.rts=False
port.port='/dev/ttyUSB0'
port.open()
port.rts=True
time.sleep(.1)
port.rts=False
start=time.monotonic()
rows=0
with path.open('wb') as output:
    while time.monotonic()-start<1200:
        line=port.readline()
        if not line: continue
        output.write(line)
        output.flush()
        decoded=line.decode(errors='replace').strip()
        if decoded.startswith('READY,'): port.write(command.encode())
        if decoded.startswith('DATA,'):
            rows+=1
            if rows%24==0: print(f'{path.stem}: {rows} rows, {time.monotonic()-start:.1f}s',flush=True)
        elif not decoded.startswith(('VERIFY,','MEM,')):
            print(decoded,flush=True)
        if decoded.startswith('END,'):
            print(f'Completed {path.name}: {rows} rows',flush=True)
            break
        if 'abort() was called' in decoded or 'Guru Meditation' in decoded or decoded.startswith('FAIL:'):
            raise RuntimeError(decoded)
    else: raise TimeoutError(f'{path.name}: {rows} rows')
port.close()
