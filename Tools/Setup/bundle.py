"""Create the optional friend payload from a release and the user's Address Library ZIP."""
from pathlib import Path
import argparse, hashlib, zipfile
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('release', type=Path); p.add_argument('output', type=Path)
p.add_argument('--address-library', type=Path)
a = p.parse_args()
assert a.release.resolve() != a.output.resolve() and not a.output.exists(), 'Choose a new output file'
files = {}
with zipfile.ZipFile(a.release) as z:
    files = {n: z.read(n) for n in z.namelist() if not n.endswith('/') and n != 'SHA256SUMS.txt'}
for source in Path(__file__).parent.iterdir():
    if source.is_file(): files['source/Setup/' + source.name] = source.read_bytes()
files['COOP-QUICKSTART.txt'] = (Path(__file__).resolve().parents[3] / 'COOP-QUICKSTART.txt').read_bytes()
if a.address_library:
    name = 'SKSE/Plugins/versionlib-1-7-104-0.bin'
    with zipfile.ZipFile(a.address_library) as z: files[name] = z.read(name)
    files['SkyrimTogetherReborn/AddressLibrary-README.txt'] = (
        'Address Library for SKSE Plugins v13, for Skyrim 1.7.104.0.\n'
        'Included from the user-provided archive: ' + a.address_library.name + '\n'
        'Original project: https://www.nexusmods.com/skyrimspecialedition/mods/32444\n'
        'Only the database matching this build is included.\n').encode()
files['SHA256SUMS.txt'] = ''.join(hashlib.sha256(data).hexdigest() + '  ' + name + '\n' for name, data in sorted(files.items())).encode()
with zipfile.ZipFile(a.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for name, data in sorted(files.items()): z.writestr(name, data)
print(a.output, a.output.stat().st_size)
