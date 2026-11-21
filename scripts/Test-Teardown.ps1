param([string]$Build=(Join-Path $PSScriptRoot '../build'),[int]$Instances=8,[int]$Cycles=3)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$Build=[IO.Path]::GetFullPath($Build)
$product=[regex]::Match((Get-Content (Join-Path $root 'CMakeLists.txt') -Raw),'project\((\w+) VERSION').Groups[1].Value
$hostExe=Join-Path $Build ($product+'TeardownHost_artefacts/Release/'+$product+'TeardownHost.exe')
$plugin=Join-Path $Build ($product+'_artefacts/Release/VST3/'+$product+'.vst3')
$folder=Join-Path $Build 'verification/teardown'
New-Item -ItemType Directory -Force $folder | Out-Null
foreach($mode in 'headless','editors','worker-request'){
    $report=Join-Path $folder ($mode+'.json');$log=Join-Path $folder ($mode+'.log')
    $arguments=@('--a',('"'+$plugin+'"'),'--instances',"$Instances",'--cycles',"$Cycles",'--settle-ms','1500','--report',('"'+$report+'"'),'--log',('"'+$log+'"'))
    if($mode -ne 'headless'){$arguments+='--gui'}
    if($mode -eq 'worker-request'){$arguments+='--worker-close'}
    $process=Start-Process -FilePath $hostExe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(60000)){$process.Kill();throw "Owned teardown host did not exit: $mode. Stage log: $log"}
    if($process.ExitCode -ne 0){throw "Owned teardown host crashed: $mode. Stage log: $log"}
    $result=Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
    if(!$result.passed){throw "Teardown report failed: $mode"}
    if(!@($result.module_observations | Where-Object {!$_.module_still_mapped}).Count){throw "No complete module unload was exercised: $mode"}
    Write-Output "$product $mode : $Instances instances x $Cycles project closes; maximum removal $($result.max_project_removal_ms) ms; child exited normally."
}
