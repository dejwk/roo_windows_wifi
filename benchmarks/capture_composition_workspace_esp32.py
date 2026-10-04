import pathlib
import sys
import time
import serial
path = pathlib.Path(sys.argv[1])
port = serial.Serial(port=None, baudrate=115200, timeout=1)
port.dtr = False
port.rts = False
port.port = '/dev/ttyUSB0'
port.open()
# Reset into the flashed application after the capture port is ready.
port.rts = True
time.sleep(0.1)
port.rts = False
start = time.monotonic()
lines = 0
with path.open('wb') as output:
    while time.monotonic() - start < 600:
        line = port.readline()
        if not line:
            continue
        output.write(line)
        output.flush()
        decoded = line.decode('utf-8', errors='replace').strip()
        if decoded.startswith('DATA,'):
            lines += 1
            if lines % 24 == 0:
                print(f'Captured {lines} rows ({time.monotonic()-start:.1f}s)', flush=True)
        else:
            print(decoded, flush=True)
        if decoded.startswith('END,'):
            print(f'Completed: {lines} rows, saved {path}', flush=True)
            break
        if 'abort() was called' in decoded or 'Guru Meditation' in decoded or decoded.startswith('FAIL:'):
            raise RuntimeError(decoded)
    else:
        raise TimeoutError(f'Benchmark did not finish; captured {lines} rows')
port.close()
