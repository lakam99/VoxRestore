"""Publish only fully uploaded, checksum-verified stable release assets."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--tag',required=True)
parser.add_argument('--directory',required=True)
args=parser.parse_args()
assert re.fullmatch(r'v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)',args.tag)
root=Path(__file__).resolve().parent.parent
folder=Path(args.directory).resolve()
version=args.tag[1:]
names=[f'VoxRestore-{version}-Windows-x64.zip',f'VoxRestore-{version}-source.zip','SHA256SUMS.txt']
names.insert(-1,f'VoxRestore-{version}-macOS-arm64.zip')
expected={}
for name in names:
    path=folder/name
    with path.open('rb') as stream:
        expected[name]={'sha256':hashlib.file_digest(stream,'sha256').hexdigest(),'size':path.stat().st_size}
lines=(folder/'SHA256SUMS.txt').read_text().splitlines()
assert set(lines)=={f"{expected[name]['sha256']}  {name}" for name in names[:-1]}

def gh(*arguments,check=True):
    result=subprocess.run(['gh',*arguments],capture_output=True,text=True)
    if check and result.returncode:
        raise RuntimeError(result.stderr.strip())
    return result

repository='lakam99/VoxRestore'
existing=gh('release','view',args.tag,'--repo',repository,'--json','isDraft',check=False)
published=existing.returncode==0 and not json.loads(existing.stdout)['isDraft']
if not published:
    if existing.returncode:
        notes=root/'docs'/f'Release-{version}.md'
        note_args=['--notes-file',str(notes)] if notes.exists() else ['--generate-notes']
        gh('release','create',args.tag,'--repo',repository,'--verify-tag','--draft','--title',f'VoxRestore {version}',*note_args)
    gh('release','upload',args.tag,'--repo',repository,'--clobber',*[str(folder/name) for name in names])
pages=json.loads(gh('api',f'repos/{repository}/releases','--paginate','--slurp').stdout)
release=next(release for page in pages for release in page if release['tag_name']==args.tag)
actual={asset['name']:asset for asset in release['assets']}
assert set(actual)==set(expected),'Unexpected or missing release assets.'
for name,metadata in expected.items():
    asset=actual[name]
    assert asset['state']=='uploaded' and asset['size']==metadata['size']
    assert asset['digest']=='sha256:'+metadata['sha256'],f'Remote digest mismatch: {name}'
if not published:
    gh('release','edit',args.tag,'--repo',repository,'--draft=false','--latest')
print(f'https://github.com/{repository}/releases/tag/{args.tag}')
