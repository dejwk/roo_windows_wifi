"""Prepare isolated historical benchmark builds without modifying source repos.

Usage: python3 prepare.py /path/to/roo /tmp/display-history
Then explicitly run: <PlatformIO python> run.py /tmp/display-history
The latter builds, flashes, and measures every checkpoint on /dev/ttyUSB0.
"""
import hashlib
import io
import json
import pathlib
import shutil
import subprocess
import sys
import tarfile

repos = pathlib.Path(sys.argv[1]).resolve()
output = pathlib.Path(sys.argv[2]).resolve()
source = pathlib.Path(__file__).resolve().parent
output.mkdir(parents=True)
revisions = [
    'c243c4fc45cc8c86ed05abd9e04e529fa02a4fc8',
    'dbdea8e74851bef6fff3453685bde8b92a8d613e',
    'c61724a096ccdbfc3802a8ce60b8efdacf974ef1',
    '6b348f342628640ef4507b54afd9ef9210a6b9b4',
    'e745df95202100e246366114139ce86d32b9a37a',
    'b985f1168f45354276a58f9e9904585b93f64c48',
]
for library, checkpoints in [
    ('roo_display', revisions),
    ('roo_windows', ['8a61b433051e29aa8c0f1ce721e05e0aa5de733c']),
]:
    for revision in checkpoints:
        destination = output/'snapshots'/revision[:7]
        destination.mkdir(parents=True)
        archive = subprocess.check_output(['git', 'archive', revision],
                                          cwd=repos/library)
        with tarfile.open(fileobj=io.BytesIO(archive)) as contents:
            contents.extractall(destination, filter='data')
shutil.copytree(output/'snapshots/c243c4f', output/'active_display')
shutil.copytree(source/'src', output/'src')
for script in ['capture.py', 'analyze.py']:
    shutil.copyfile(source/script, output/script)
project = output/'project'
(project/'src').mkdir(parents=True)
for path in (output/'src').iterdir():
    (project/'src'/path.name).symlink_to(path)
libraries = [
    'roo_backport', 'roo_collections', 'roo_display', 'roo_flags', 'roo_io',
    'roo_logging', 'roo_threads', 'roo_time', 'roo_windows', 'roo_windows_wifi',
    'roo_wifi', 'roo_prefs', 'roo_scheduler', 'roo_fonts_basic',
    'roo_fonts_material', 'roo_icons', 'roo_locale',
]
manifest = {}
for library in libraries:
    target = repos/library
    if library == 'roo_display':
        target = output/'active_display'
    elif library == 'roo_windows':
        target = output/'snapshots/8a61b43'
    wrapper = project/'lib'/library
    wrapper.mkdir(parents=True)
    (wrapper/'src').symlink_to(target/'src')
    data = json.loads((target/'library.json').read_text())
    data.pop('dependencies', None)
    (wrapper/'library.json').write_text(json.dumps(data, indent=2)+'\n')
    for support in ['scripts', 'prebuild.py']:
        if (target/support).exists():
            (wrapper/support).symlink_to(target/support)
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                                       cwd=repos/library, text=True).strip()
    digest = hashlib.sha256()
    for path in sorted((target/'src').rglob('*')):
        if path.is_file():
            digest.update(str(path.relative_to(target)).encode()+b'\0')
            digest.update(path.read_bytes())
    manifest[library] = {'local_head': revision,
                         'source_sha256': digest.hexdigest()}
(output/'dependencies.json').write_text(json.dumps(manifest, indent=2)+'\n')
platform = pathlib.Path.home()/'.platformio/platforms/espressif32'
(project/'platformio.ini').write_text(f'''[env:esp32]
platform = {platform}
board = esp32dev
board_build.partitions = huge_app.csv
framework = arduino
upload_port = /dev/ttyUSB0
upload_speed = 921600
monitor_speed = 115200
build_flags =
  -DARDUINO_LOOP_STACK_SIZE=32768
  -DNDEBUG
  -I{repos}/roo_wifi/test
''')
print(output)
