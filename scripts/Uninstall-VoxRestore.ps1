param([string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'Programs/VoxRestore'), [switch]$NoRegistration, [switch]$NoShortcuts)
. (Join-Path $PSScriptRoot 'Release-Common.ps1')
$mutex=New-Object Threading.Mutex($false,'Local\VoxRestore-Installer')
$locked=$false
try {
try {$locked=$mutex.WaitOne(0)} catch [Threading.AbandonedMutexException] {$locked=$true}
if (-not $locked) {throw 'Another VoxRestore installation is in progress.'}
$InstallRoot = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\','/')
Assert-OwnedDirectory $InstallRoot '.voxrestore-install.json'
$info = Get-Content -LiteralPath (Join-Path $InstallRoot '.voxrestore-install.json') -Raw | ConvertFrom-Json
if ($info.installRoot -ine $InstallRoot -or [IO.Path]::GetFileName($InstallRoot) -ine 'VoxRestore') { throw 'Installation path does not match its ownership marker.' }
$vst = Assert-ChildPath (Join-Path $info.vst3Root 'VoxRestore.vst3') $info.vst3Root
Assert-OwnedDirectory $vst '.voxrestore-owner.json'
Assert-Unlocked (Join-Path $InstallRoot 'VoxRestore.exe')
Assert-Unlocked (Join-Path $vst 'Contents/x86_64-win/VoxRestore.vst3')
$targets = @(@($vst,$info.vst3Root,'.voxrestore-owner.json'), @($InstallRoot,(Split-Path -Parent $InstallRoot),'.voxrestore-install.json'))
$vstBackupRoot=Join-Path (Split-Path -Parent $info.vst3Root) 'VoxRestore-Backups'
foreach ($pair in @(@($info.previousApp,(Split-Path -Parent $InstallRoot),'VoxRestore.backup-','.voxrestore-install.json'),@($info.previousVst,$vstBackupRoot,'VoxRestore.vst3.backup-','.voxrestore-owner.json'))) {
    if ($pair[0] -and (Test-Path -LiteralPath $pair[0])) {
        $candidate = Assert-ChildPath $pair[0] $pair[1]
        if (-not [IO.Path]::GetFileName($candidate).StartsWith($pair[2],[StringComparison]::Ordinal)) { throw 'Unexpected backup path.' }
        $targets += ,@($candidate,$pair[1],$pair[3])
    }
}
foreach ($pair in $targets) { if (Test-Path -LiteralPath $pair[0]) { $null=Assert-ChildPath $pair[0] $pair[1]; Assert-OwnedDirectory $pair[0] $pair[2] } }
if (-not $NoShortcuts) {
    $link = Join-Path ([Environment]::GetFolderPath('Programs')) 'VoxRestore.lnk'
    if (Test-Path -LiteralPath $link) {
        $shell = New-Object -ComObject WScript.Shell
        if ($shell.CreateShortcut($link).TargetPath -ieq (Join-Path $InstallRoot 'VoxRestore.exe')) { Remove-Item -LiteralPath $link -Force }
    }
}
if (-not $NoRegistration) {
    $key='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\VoxRestore'
    if ((Get-ItemProperty -LiteralPath $key -Name InstallLocation -ErrorAction SilentlyContinue).InstallLocation -ieq $InstallRoot) { Remove-Item -LiteralPath $key -Recurse }
}
foreach ($pair in $targets) { if (Test-Path -LiteralPath $pair[0]) { $null=Assert-ChildPath $pair[0] $pair[1]; Remove-Item -LiteralPath $pair[0] -Recurse -Force } }
Write-Output 'VoxRestore removed. User settings and recordings were preserved.'
} finally {if ($locked) {$mutex.ReleaseMutex()};$mutex.Dispose()}
