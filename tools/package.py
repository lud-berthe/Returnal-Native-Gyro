"""Create the Nexus payload with one consolidated license file."""
from pathlib import Path
import argparse, hashlib, json, re, zipfile
ROOT = Path(__file__).resolve().parents[1]
def sha(data): return hashlib.sha256(data).hexdigest()
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build')
    parser.add_argument('--sdl-dll', type=Path, default=ROOT/'_deps/SDL3-3.4.16/lib/x64/SDL3.dll')
    parser.add_argument('--output-dir', type=Path, default=ROOT/'dist')
    args = parser.parse_args()
    version = re.search(r'project\(ReturnalGyro VERSION ([0-9.]+)', (ROOT/'CMakeLists.txt').read_text()).group(1)
    files = {
        'Returnal/Binaries/Win64/version.dll': (args.build_dir/'loader/Release/version.dll').read_bytes(),
        'Returnal/Binaries/Win64/ReturnalGyro.dll': (args.build_dir/'Release/ReturnalGyro.dll').read_bytes(),
        'Returnal/Binaries/Win64/SDL3.dll': args.sdl_dll.read_bytes(),
        'LICENSES.txt': (ROOT/'LICENSES.txt').read_bytes(),
    }
    dep = next(d for d in json.loads((ROOT/'dependencies.json').read_text())['dependencies'] if d['name']=='SDL')
    if sha(files['Returnal/Binaries/Win64/SDL3.dll']) != dep['runtimeSHA256']:
        raise SystemExit('Unexpected SDL runtime; refusing to package.')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    archive = args.output_dir/f'ReturnalGyro-{version}.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, date_time=(2026,9,17,0,0,0))
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, data)
    manifest = {'version': version, 'archive': {'name': archive.name, 'sha256': sha(archive.read_bytes())}, 'files': {n:sha(d) for n,d in sorted(files.items())}}
    (args.output_dir/f'ReturnalGyro-{version}-SHA256.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    print(archive)
    print('SHA256:',manifest['archive']['sha256'])
if __name__ == '__main__': main()
