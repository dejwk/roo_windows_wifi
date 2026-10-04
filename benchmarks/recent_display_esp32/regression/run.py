"""Run the rounded-paint layout controls in a fresh prepared history directory.

Usage: <PlatformIO python> regression/run.py PREPARED_DIRECTORY [FIRST_VARIANT]
Prepare the directory with ../prepare.py first. This replaces /dev/ttyUSB0
firmware, without saving or restoring it. Source repositories are not modified.
"""
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(sys.argv[1]).resolve()
source = Path(__file__).resolve().parent
platformio = Path.home()/'.platformio'
bin_dir = platformio/'penv/bin'
build = root/'project/.pio/build/esp32'
stack_path = Path('src/roo_display/composition/rasterizable_stack.cpp')
snapshot = root/'snapshots/b985f11'
section = '.text._ZNK11roo_display17RasterizableStack13readColorRectEssssPNS_5ColorE'
variants = ['runtime_gate', 'hot0000', 'hot0016', 'hot0064', 'hot0256',
            'hotnorm', 'reloc00']


def setup_probe():
    """Restore the measured base sources and apply only diagnostic changes."""
    shutil.copytree(snapshot, root/'active_display', dirs_exist_ok=True)
    for path in (source.parent/'src').iterdir():
        shutil.copyfile(path, root/'src'/path.name)
    padding = root/'src/padding.cpp'
    if padding.exists():
        padding.unlink()
    subprocess.run(['patch', '-p1', '-i', str(source/'probe.patch')],
                   cwd=root, check=True)
    link = root/'project/src/padding.cpp'
    if not link.is_symlink():
        link.symlink_to(padding)


setup_probe()
gated_stack = (root/'active_display'/stack_path).read_text()
start = variants.index(sys.argv[2]) if len(sys.argv) > 2 else 0
for label in variants[start:]:
    text = gated_stack
    commands = [('regression', 'R')]
    if label == 'reloc00':
        for path in (source.parent/'src').iterdir():
            shutil.copyfile(path, root/'src'/path.name)
        (root/'src/padding.cpp').unlink()
        (root/'project/src/padding.cpp').unlink()
        text = (snapshot/stack_path).read_text()
        text = (f'asm(".pushsection {section},\\"ax\\",@progbits\\n '
                '.balign 4\\n .popsection\\n");\n') + text
        commands = [('synthetic', 'S'), ('wifi', 'W')]
    elif label.startswith('hot') and label != 'hotnorm':
        count = int(label[3:])
        text = (f'asm(".pushsection {section},\\"ax\\",@progbits\\n '
                f'.balign 4\\n .space {count}\\n .popsection\\n");\n') + text
    (root/'active_display'/stack_path).write_text(text)
    (root/'src/revision.cpp').write_text(
        f'const char* benchmark_revision = "{label}";\n')
    print('BUILD '+label, flush=True)
    with (root/f'build_{label}.log').open('w') as log:
        subprocess.run([str(bin_dir/'pio'), 'run', '-d', str(root/'project'),
                        '-j', '10'], stdout=log, stderr=subprocess.STDOUT,
                       check=True)
    archive = root/'firmware'/label
    archive.mkdir(parents=True, exist_ok=True)
    for name in ['firmware.bin', 'firmware.elf', 'bootloader.bin', 'partitions.bin']:
        shutil.copyfile(build/name, archive/name)
    print('FLASH '+label, flush=True)
    with (root/f'upload_{label}.log').open('w') as log:
        subprocess.run([
            str(bin_dir/'esptool.py'), '--chip', 'esp32', '--port', '/dev/ttyUSB0',
            '--baud', '921600', '--before', 'default-reset', '--after',
            'hard-reset', 'write-flash', '-z', '--flash-mode', 'dio',
            '--flash-freq', '40m', '--flash-size', '4MB',
            '0x1000', str(archive/'bootloader.bin'),
            '0x8000', str(archive/'partitions.bin'),
            '0xe000', str(platformio/'packages/framework-arduinoespressif32/'
                          'tools/partitions/boot_app0.bin'),
            '0x10000', str(archive/'firmware.bin'),
        ], stdout=log, stderr=subprocess.STDOUT, check=True)
    for suite, command in commands:
        name = label if suite == 'regression' else f'{label}_{suite}'
        subprocess.run([str(bin_dir/'python'), str(root/'capture.py'),
                        str(root/f'{name}.log'), command], check=True)
print('ALL CONTROLS COMPLETE', flush=True)
