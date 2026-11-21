from pathlib import Path
import json,platform,subprocess,uuid
root=Path(__file__).resolve().parents[1];build=root/'build';reports=build/'verification-mac';reports.mkdir(exist_ok=True)
app=build/'VoxRestoreApp_artefacts/Release/VoxRestore.app/Contents/MacOS/VoxRestore'
plugin=build/'VoxRestore_artefacts/Release/VST3/VoxRestore.vst3'
assert platform.machine()=='arm64'
def check(name,*args):
    report=reports/(name+'.json');command=[str(app),*map(str,args),'--report',str(report)]
    try:subprocess.run(command,check=True,timeout=120)
    except subprocess.CalledProcessError:
        subprocess.run(['lldb','--batch','-o','run','-k','thread backtrace all','--',*command],timeout=180);raise
    result=json.loads(report.read_text());assert result.get('passed'),result
reference=root/'resources/tests/clean-reference.wav';voice=root/'resources/tests/clean-evaluation.wav'
check('engine','--self-test','--folder',reports)
check('updater-retirement','--updater-test')
check('vst3','--validate-plugin',plugin,'--reference',reference)
check('workflow','--workflow',voice,reports/f'export-{uuid.uuid4().hex}.wav','--reference',reference)
for skin,name in enumerate(('ivory','graphite','walnut')):check(name,'--render-ui',reports/(name+'.png'),'--skin',skin,'--reference',reference)
for bundle in (app.parents[2],plugin):
    subprocess.run(['codesign','--verify','--deep','--strict',str(bundle)],check=True)
    binary=app if bundle.suffix=='.app' else bundle/'Contents/MacOS/VoxRestore'
    assert subprocess.check_output(['lipo','-archs',str(binary)],text=True).strip()=='arm64'
(reports/'platform.json').write_text(json.dumps({'passed':True,'architecture':platform.machine(),'macos':platform.mac_ver()[0],'minimum_macos':'14.0','developer_id_signed':False,'notarized':False},indent=2))
print('Apple Silicon DSP, VST3, background analysis, reference persistence, WAV export, raster UI and bundle signatures passed.')
