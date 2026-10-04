"""Build, flash, and measure six checkpoints on the connected ESP32.

Usage: <PlatformIO python> run.py PREPARED_DIRECTORY [FIRST_REVISION]
Flashing replaces the connected board firmware; no backup/restore is performed.
"""
import pathlib
import shutil
import subprocess
import sys

root = pathlib.Path(sys.argv[1]).resolve()
revisions = ['c243c4f', 'dbdea8e', 'c61724a', '6b348f3', 'e745df9', 'b985f11']
platformio = pathlib.Path.home()/'.platformio'
pio = str(platformio/'penv/bin/pio')
python = str(platformio/'penv/bin/python')
esptool = str(platformio/'penv/bin/esptool.py')
build = root/'project/.pio/build/esp32'
framework = platformio/'packages/framework-arduinoespressif32'


def select(revision):
    """Update only changed snapshot files, retaining incremental build reuse."""
    source = root/'snapshots'/revision
    active = root/'active_display'
    for path in active.rglob('*'):
        if path.is_file() and not (source/path.relative_to(active)).exists():
            path.unlink()
    for path in source.rglob('*'):
        destination = active/path.relative_to(source)
        if path.is_dir():
            destination.mkdir(exist_ok=True)
        elif not destination.exists() or path.read_bytes() != destination.read_bytes():
            shutil.copyfile(path, destination)
    (root/'src/revision.cpp').write_text(
        f'const char* benchmark_revision = "{revision}";\n')


def start_build(revision):
    select(revision)
    log = (root/f'build_{revision}.log').open('w')
    process = subprocess.Popen(
        [pio, 'run', '-d', str(root/'project'), '-j', '10'],
        stdout=log, stderr=subprocess.STDOUT)
    return process, log


start = revisions.index(sys.argv[2]) if len(sys.argv) > 2 else 0
pending = start_build(revisions[start])
for index in range(start, len(revisions)):
    revision = revisions[index]
    process, log = pending
    if process.wait() != 0:
        raise RuntimeError(f'Build failed: {revision}')
    log.close()
    archive = root/'firmware'/revision
    archive.mkdir(parents=True, exist_ok=True)
    for name in ['firmware.bin', 'firmware.elf', 'bootloader.bin', 'partitions.bin']:
        shutil.copyfile(build/name, archive/name)
    print(f'FLASH {revision}', flush=True)
    with (root/f'upload_{revision}.log').open('w') as log:
        subprocess.run([
            esptool, '--chip', 'esp32', '--port', '/dev/ttyUSB0',
            '--baud', '921600', '--before', 'default-reset',
            '--after', 'hard-reset', 'write-flash', '-z',
            '--flash-mode', 'dio', '--flash-freq', '40m', '--flash-size', '4MB',
            '0x1000', str(archive/'bootloader.bin'),
            '0x8000', str(archive/'partitions.bin'),
            '0xe000', str(framework/'tools/partitions/boot_app0.bin'),
            '0x10000', str(archive/'firmware.bin'),
        ], stdout=log, stderr=subprocess.STDOUT, check=True)
    # Compilation uses the host while the board runs its already archived image.
    # Flashing and serial capture remain strictly sequential.
    if index + 1 < len(revisions):
        pending = start_build(revisions[index + 1])
    for suite, command in [('synthetic', 'S'), ('wifi', 'W')]:
        subprocess.run([
            python, str(root/'capture.py'),
            str(root/f'{revision}_{suite}.log'), command,
        ], check=True)
print('ALL REVISIONS COMPLETE', flush=True)
