"""Stage native macOS bundles without losing executable modes or signatures."""
from pathlib import Path
import hashlib,json,re,shutil,subprocess

root=Path(__file__).resolve().parent.parent
version=re.search(r'project\(VoxRestore VERSION ([0-9.]+)',(root/'CMakeLists.txt').read_text())[1]
out=root/'dist-mac';out.mkdir(exist_ok=True)
stage=out/'VoxRestore';assert not stage.exists();stage.mkdir()
for source in ('build/VoxRestoreApp_artefacts/Release/VoxRestore.app','build/VoxRestore_artefacts/Release/VST3/VoxRestore.vst3'):
    destination=stage/Path(source).name
    subprocess.run(['ditto',str(root/source),str(destination)],check=True)
    subprocess.run(['codesign','--verify','--deep','--strict',str(destination)],check=True)
shutil.copy2(root/'docs/macOS-user-guide.txt',stage/'README.txt')
shutil.copytree(root/'Licenses',stage/'Licenses')
shutil.copytree(root/'build/verification-mac',stage/'Verification')
manifest={'schema':1,'version':version,'platform':'macos-arm64','minimum_macos':'14.0','developer_id_signed':False,'notarized':False,'files':{}}
for bundle in ('VoxRestore.app','VoxRestore.vst3'):
    for path in sorted((stage/bundle).rglob('*')):
        if path.is_file():manifest['files'][path.relative_to(stage).as_posix()]=hashlib.sha256(path.read_bytes()).hexdigest()
(stage/'release-manifest-mac.json').write_text(json.dumps(manifest,indent=2))
archive=out/f'VoxRestore-{version}-macOS-arm64.zip'
subprocess.run(['ditto','-c','-k','--sequesterRsrc','--keepParent',str(stage),str(archive)],check=True)
(out/'SHA256SUMS-mac.txt').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
print(archive)
