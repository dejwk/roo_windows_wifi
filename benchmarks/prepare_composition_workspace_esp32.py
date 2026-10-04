"""Prepare three isolated PlatformIO projects from local roo repositories.

Usage: python3 prepare.py /path/to/roo /tmp/wifi-benchmark
Build each project with `pio run -d <output>/<baseline|fresh|reuse>`.
Flashing is a separate explicit `pio run -d <project> -t upload` command.
"""
import io
import json
import pathlib
import shutil
import subprocess
import sys
import tarfile

repos = pathlib.Path(sys.argv[1]).resolve()
output = pathlib.Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
source = pathlib.Path(__file__).with_name('composition_workspace_esp32.cpp')
shutil.copyfile(source, output/'benchmark.cpp')
revisions = {'roo_display': 'b985f1168f45354276a58f9e9904585b93f64c48',
             'roo_windows': '8a61b433051e29aa8c0f1ce721e05e0aa5de733c'}
for library, revision in revisions.items():
    archive = subprocess.check_output(['git','archive',revision], cwd=repos/library)
    destination = output/('baseline_'+library)
    destination.mkdir()
    with tarfile.open(fileobj=io.BytesIO(archive)) as contents:
        contents.extractall(destination, filter='data')

shutil.copytree(repos/'roo_windows', output/'current_windows',
                ignore=shutil.ignore_patterns('.git','bazel-*'))
header = output/'current_windows/src/roo_windows/core/clipper.h'
text = header.read_text()
for arguments in ['*this', '*this, clip']:
    old = f'return workspace_.createStream({arguments});'
    assert text.count(old) == 1, 'OverlayStack changed; update this control patch.'
    base_arguments = '' if arguments == '*this' else 'clip'
    text = text.replace(old, '#ifdef ROO_BENCHMARK_DISABLE_WORKSPACE\n'
                        f'    return RasterizableStack::createStream({base_arguments});\n'
                        '#else\n    '+old+'\n#endif')
header.write_text(text)

libraries = ['roo_backport','roo_collections','roo_display','roo_flags','roo_io',
             'roo_logging','roo_threads','roo_time','roo_windows','roo_windows_wifi',
             'roo_wifi','roo_prefs','roo_scheduler','roo_fonts_basic',
             'roo_fonts_material','roo_icons','roo_locale']
platform = pathlib.Path.home()/'.platformio/platforms/espressif32'
for variant in ['baseline','fresh','reuse']:
    project = output/variant
    (project/'src').mkdir(parents=True)
    (project/'lib').mkdir()
    (project/'src/main.cpp').symlink_to(output/'benchmark.cpp')
    for library in libraries:
        target = repos/library
        if variant == 'baseline' and library in revisions:
            target = output/('baseline_'+library)
        elif library == 'roo_windows':
            target = output/'current_windows'
        wrapper = project/'lib'/library
        wrapper.mkdir()
        (wrapper/'src').symlink_to(target/'src')
        manifest = target/'library.json'
        data = json.loads(manifest.read_text()) if manifest.exists() else {'name':library,'version':'0.0.0'}
        # Use local sources without downloading duplicate registry libraries.
        data.pop('dependencies',None)
        (wrapper/'library.json').write_text(json.dumps(data,indent=2)+'\n')
        for support in ['scripts','prebuild.py']:
            if (target/support).exists(): (wrapper/support).symlink_to(target/support)
    flags = ('  -DARDUINO_LOOP_STACK_SIZE=32768\n  -DNDEBUG\n'
             f'  -I{repos}/roo_wifi/test\n'
             f'  -DROO_BENCHMARK_VARIANT=\\"{variant}\\"\n')
    if variant == 'fresh': flags += '  -DROO_BENCHMARK_DISABLE_WORKSPACE\n'
    (project/'platformio.ini').write_text(
        f'[env:esp32]\nplatform = {platform}\nboard = esp32dev\n'
        'board_build.partitions = huge_app.csv\nframework = arduino\n'
        'upload_port = /dev/ttyUSB0\nupload_speed = 921600\n'
        'monitor_speed = 115200\nbuild_flags =\n'+flags)
if len(sys.argv) > 3 and sys.argv[3] == '--probe':
    shutil.copyfile(output/'benchmark.cpp', output/'probe.cpp')
    shutil.copytree(repos/'roo_display', output/'probe_display',
                    ignore=shutil.ignore_patterns('.git','bazel-*'))
    patch = pathlib.Path(__file__).with_name('composition_workspace_esp32_probe.patch')
    subprocess.run(['patch','-p1','-i',str(patch.resolve())],cwd=output,check=True)
    main = output/'reuse/src/main.cpp'
    main.unlink()
    main.symlink_to(output/'probe.cpp')
    display = output/'reuse/lib/roo_display/src'
    display.unlink()
    display.symlink_to(output/'probe_display/src')
print(output)
