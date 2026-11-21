from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess,zipfile
root=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser();parser.add_argument('--build',default='build');parser.add_argument('--output',default='dist');parser.add_argument('--tag');options=parser.parse_args()
if (root/'.git/index.lock').exists():raise SystemExit('Git staging is still running; package only a complete source index.')
build=(root/options.build).resolve();out=(root/options.output).resolve();version=re.search(r'project\(VoxRestore VERSION ([0-9.]+)',(root/'CMakeLists.txt').read_text())[1]
if options.tag and options.tag!='v'+version:raise SystemExit('Tag/version mismatch.')
stage=out/'VoxRestore'
if stage.exists():raise SystemExit('Choose a fresh package directory.')
stage.mkdir(parents=True)
shutil.copytree(build/'VoxRestoreApp_artefacts/Release',stage/'Standalone',ignore=shutil.ignore_patterns('*.pdb','*.ilk','*.lib','*.exp'))
shutil.copytree(build/'VoxRestore_artefacts/Release/VST3/VoxRestore.vst3',stage/'VST3/VoxRestore.vst3')
for name in ('Install-VoxRestore.ps1','Release-Common.ps1'):shutil.copy2(root/'scripts'/name,stage/name)
shutil.copytree(root/'Licenses',stage/'Licenses');shutil.copytree(root/'resources/generated',stage/'Artwork')
shutil.copy2(root/'README.md',stage/'README.md');shutil.copy2(root/'docs/Windows-user-guide.txt',stage/'README.txt')
for source in (build/'verification').glob('*'):
    if source.suffix not in ('.json','.png'):continue
    dest=stage/('Previews' if source.suffix=='.png' else 'Verification')/source.name;dest.parent.mkdir(exist_ok=True);shutil.copy2(source,dest)
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest={'schema':1,'product':'audio.arkam.voxrestore','repository':'lakam99/VoxRestore','version':version,'platform':'windows-x64','files':{}}
for directory in ('Standalone','VST3'):
    for p in sorted((stage/directory).rglob('*')):
        if p.is_file():manifest['files'][p.relative_to(stage).as_posix()]=digest(p)
(stage/'release-manifest.json').write_text(json.dumps(manifest,indent=2))
source=out/f'VoxRestore-{version}-source.zip'
tracked=subprocess.check_output(['git','ls-files','-z'],cwd=root).decode().split('\0')
required={'CMakeLists.txt','Source/VoxDsp.cpp','LICENSE','third_party/JUCE/CMakeLists.txt','third_party/onnxruntime/runtimes/win-x64/native/onnxruntime.dll','resources/silero_vad.onnx'}
if not required.issubset(set(tracked)):raise SystemExit('Source index is incomplete: '+', '.join(sorted(required-set(tracked))))
vendor_files={p.relative_to(root).as_posix() for p in (root/'third_party').rglob('*') if p.is_file()}
missing_vendor=vendor_files-set(tracked)
if missing_vendor:raise SystemExit('Vendored dependencies omitted from source index: '+', '.join(sorted(missing_vendor)[:10]))
with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
    for name in sorted(filter(None,tracked)):
        p=root/name
        if p.is_file():
            assert p.name!='google-services.json','Unused Firebase config must not ship.'
            archive.write(p,'VoxRestore-source/'+name)
shutil.copy2(source,stage/'VoxRestore-source.zip')
binary=out/f'VoxRestore-{version}-Windows-x64.zip'
with zipfile.ZipFile(binary,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
    for p in sorted(stage.rglob('*')):
        if p.is_file():archive.write(p,p.relative_to(out))
for p in (source,binary):
    with zipfile.ZipFile(p) as archive:assert archive.testzip() is None
(out/'SHA256SUMS.txt').write_text(''.join(f'{digest(p)}  {p.name}\n' for p in (binary,source)))
print(json.dumps({'version':version,'windows':str(binary),'source':str(source)}))
