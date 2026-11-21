param([string]$TestRoot=(Join-Path ([IO.Path]::GetTempPath()) ('VoxRestore-progress-'+[guid]::NewGuid().ToString('N'))))
$ErrorActionPreference='Stop'
$null=New-Item -ItemType Directory -Path $TestRoot -Force
$tokens=$null;$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'VoxRestore-Update.ps1'),[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw $parseErrors[0]}
$definition=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Write-UpdateProgress'},$true)
if(-not $definition){throw 'Update progress writer was not found.'}
Invoke-Expression $definition.Extent.Text
$Report=Join-Path $TestRoot 'report.json'
foreach($fraction in 0,.25,.75,1){
    Write-UpdateProgress 1 $fraction ([long]($fraction*1000)) 1000
    $actual=Get-Content -LiteralPath ($Report+'.progress.json') -Raw | ConvertFrom-Json
    if($actual.phase -ne 1 -or $actual.fraction -ne $fraction -or $actual.received -ne [long]($fraction*1000)){throw 'Download progress replacement/readback failed.'}
}
foreach($phase in 2,3,4){Write-UpdateProgress $phase;$actual=Get-Content -LiteralPath ($Report+'.progress.json') -Raw | ConvertFrom-Json;if($actual.phase -ne $phase -or $actual.fraction -ne -1){throw 'Indeterminate update phase failed.'}}
Write-UpdateProgress 5 1
$actual=Get-Content -LiteralPath ($Report+'.progress.json') -Raw | ConvertFrom-Json
if($actual.phase -ne 5 -or $actual.fraction -ne 1 -or (Test-Path -LiteralPath ($Report+'.progress.json.tmp')) -or (Test-Path -LiteralPath ($Report+'.progress.json.previous'))){throw 'Completed update progress was not committed cleanly.'}
@{passed=$true;checks=8}|ConvertTo-Json|Set-Content (Join-Path $TestRoot 'progress-tests.json') -Encoding UTF8
Write-Output 'Update progress: 8 atomic write/read and phase checks passed.'
