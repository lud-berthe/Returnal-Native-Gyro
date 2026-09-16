"""Fetch the pinned official SDL development archive; verify before extracting."""
from pathlib import Path
import argparse, hashlib, json, tempfile, urllib.request, zipfile
ROOT = Path(__file__).resolve().parents[1]
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--archive', type=Path)
    args = parser.parse_args()
    dep = next(d for d in json.loads((ROOT/'dependencies.json').read_text())['dependencies'] if d['name'] == 'SDL')
    destination = ROOT/'_deps'
    destination.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='sdl-', dir=destination) as temp:
        archive = args.archive
        if archive is None:
            archive = Path(temp)/'SDL.zip'
            print('Downloading', dep['download'])
            request = urllib.request.Request(dep['download'], headers={'User-Agent': 'ReturnalGyro-build'})
            with urllib.request.urlopen(request, timeout=120) as response:
                archive.write_bytes(response.read())
        if hashlib.sha256(archive.read_bytes()).hexdigest() != dep['sha256']:
            raise SystemExit('SDL SHA-256 mismatch; nothing extracted.')
        with zipfile.ZipFile(archive) as z:
            for name in z.namelist():
                target = (destination/name).resolve()
                if not target.is_relative_to(destination.resolve()):
                    raise SystemExit('Invalid archive path.')
            z.extractall(destination)
    print('SDL verified and extracted into _deps/.')
if __name__ == '__main__': main()
