param([string]$Build=(Join-Path $PSScriptRoot '../build'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$Build=[IO.Path]::GetFullPath($Build)
$app=Join-Path $Build 'VoxRestoreApp_artefacts/Release/VoxRestore.exe'
$plugin=Join-Path $Build 'VoxRestore_artefacts/Release/VST3/VoxRestore.vst3'
$folder=Join-Path $Build 'verification'
$null=New-Item -ItemType Directory -Path $folder -Force
$reference=Join-Path $root 'resources/tests/clean-reference.wav'
$voice=Join-Path $root 'resources/tests/clean-evaluation.wav'
function Check([string]$Name,[string[]]$Arguments){
    $report=Join-Path $folder ($Name+'.json')
    $quoted=@($Arguments|ForEach-Object {'"'+$_+'"'})+@('--report',('"'+$report+'"'))
    $process=Start-Process -FilePath $app -ArgumentList $quoted -WindowStyle Hidden -PassThru
    if(-not $process.WaitForExit(120000)){$process.Kill();throw ('Owned native test timed out: '+$Name)}
    if($process.ExitCode -ne 0){throw ('Native test failed: '+$Name)}
    $result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
    if(-not $result.passed){throw ('Native report failed: '+($result|ConvertTo-Json -Depth 8))}
}
$before=(Get-FileHash -LiteralPath $voice -Algorithm SHA256).Hash
Check 'engine' @('--self-test','--folder',$folder)
Check 'updater-retirement' @('--updater-test')
Check 'vst3' @('--validate-plugin',$plugin,'--reference',$reference)
Check 'workflow' @('--workflow',$voice,(Join-Path $folder ('export-'+[guid]::NewGuid().ToString('N')+'.wav')),'--reference',$reference)
foreach($skin in 0..2){$name=@('ivory','graphite','walnut')[$skin]
    Check $name @('--render-ui',(Join-Path $folder ($name+'.png')),'--skin',"$skin",'--reference',$reference)
    Check ($name+'-standalone') @('--render-ui',(Join-Path $folder ($name+'-standalone.png')),'--standalone','--skin',"$skin",'--reference',$reference)
}
if($before -cne (Get-FileHash -LiteralPath $voice -Algorithm SHA256).Hash){throw 'The source voice file changed.'}
Write-Output 'Engine, real VST3, editor-closed reference analysis, manual re-analysis, source-safe export and all raster skins passed.'
