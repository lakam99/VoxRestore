param([Parameter(Mandatory=$true)][string]$PackageRoot, [string]$TestRoot = (Join-Path $env:TEMP ('VoxRestore-tests-'+[guid]::NewGuid().ToString('N'))))
. (Join-Path $PSScriptRoot 'Release-Common.ps1')
$TestRoot=[IO.Path]::GetFullPath($TestRoot)
$null=New-Item -ItemType Directory -Path $TestRoot -Force
$install=Join-Path $TestRoot 'Programs/VoxRestore'
$vstRoot=Join-Path $TestRoot 'VST3'
$installer=Join-Path $PSScriptRoot 'Install-VoxRestore.ps1'
$checks=0
function Assert-True([bool]$Condition,[string]$Message) { if (-not $Condition) {throw $Message}; $script:checks++ }
function Expect-Failure([scriptblock]$Action) { $failed=$false; try { & $Action | Out-Null } catch {$failed=$true}; Assert-True $failed 'Unsafe operation unexpectedly succeeded.' }
$mock=[pscustomobject]@{ draft=$false; prerelease=$false; tag_name='v1.3.0'; assets=@(
    [pscustomobject]@{name='VoxRestore-1.3.0-Windows-x64.zip';state='uploaded';size=100;digest=('sha256:'+('0'*64));browser_download_url='https://github.com/lakam99/VoxRestore/releases/download/v1.3.0/VoxRestore-1.3.0-Windows-x64.zip'},
    [pscustomobject]@{name='SHA256SUMS.txt';state='uploaded';size=100;browser_download_url='https://github.com/lakam99/VoxRestore/releases/download/v1.3.0/SHA256SUMS.txt'}) }
Assert-True (Get-VoxRestoreRelease $mock '1.2.0').available 'Newer stable version was missed.'
Assert-True (-not (Get-VoxRestoreRelease $mock '1.3.0').available) 'Equal version was offered.'
Assert-True (-not (Get-VoxRestoreRelease $mock '2.0.0').available) 'Downgrade was offered.'
$mock.prerelease=$true; Expect-Failure {Get-VoxRestoreRelease $mock '1.2.0'}; $mock.prerelease=$false
$mock.assets[0].browser_download_url='https://example.invalid/package.zip'; Expect-Failure {Get-VoxRestoreRelease $mock '1.2.0'}
$mock.assets[0].browser_download_url='https://github.com/lakam99/VoxRestore/releases/download/v1.3.0/VoxRestore-1.3.0-Windows-x64.zip'
$mock.assets[0].digest=$null; Expect-Failure {Get-VoxRestoreRelease $mock '1.2.0'}
Expect-Failure {Get-StableVersion '1.2.3-rc1'}
$checksumLine=('0'*64)+'  fixture.zip'+"`n"
Assert-True ((Get-ReleaseChecksum $checksumLine 'fixture.zip' ('sha256:'+('0'*64))) -eq ('0'*64)) 'Text checksum manifest was rejected.'
Assert-True ((Get-ReleaseChecksum ([Text.Encoding]::UTF8.GetBytes($checksumLine)) 'fixture.zip' ('sha256:'+('0'*64))) -eq ('0'*64)) 'Windows PowerShell binary response was rejected.'
Expect-Failure {Get-ReleaseChecksum $checksumLine 'fixture.zip' ('sha256:'+('1'*64))}
Expect-Failure {Get-ReleaseChecksum ($checksumLine+$checksumLine) 'fixture.zip' ('sha256:'+('0'*64))}
Expect-Failure {Assert-ChildPath (Join-Path (Split-Path -Parent $TestRoot) 'escape') $TestRoot}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipPath=Join-Path $TestRoot 'unsafe.zip'
$zip=[IO.Compression.ZipFile]::Open($zipPath,[IO.Compression.ZipArchiveMode]::Create)
$null=$zip.CreateEntry('VoxRestore/../escape.txt');$zip.Dispose()
Expect-Failure {Expand-VerifiedArchive $zipPath (Join-Path $TestRoot 'expanded')}
& $installer -PackageRoot $PackageRoot -InstallRoot $install -Vst3Root $vstRoot -NoShortcuts -NoRegistration
Assert-True (Test-Path -LiteralPath (Join-Path $install 'VoxRestore.exe')) 'Fresh application install failed.'
Assert-True (Test-Path -LiteralPath (Join-Path $vstRoot 'VoxRestore.vst3/Contents/x86_64-win/VoxRestore.vst3')) 'Fresh VST3 install failed.'
$marker=Join-Path $install '.voxrestore-install.json'
$originalMarker=Get-Content -LiteralPath $marker -Raw
Expect-Failure {& $installer -PackageRoot $PackageRoot -InstallRoot $install -Vst3Root $vstRoot -WaitForProcess $PID -NoShortcuts -NoRegistration}
$owner=$originalMarker | ConvertFrom-Json
$owner.version='9.0.0';$owner | ConvertTo-Json | Set-Content -LiteralPath $marker -Encoding UTF8
Expect-Failure {& $installer -PackageRoot $PackageRoot -InstallRoot $install -Vst3Root $vstRoot -NoShortcuts -NoRegistration}
$originalMarker | Set-Content -LiteralPath $marker -Encoding UTF8 -NoNewline
$unknown=Join-Path $TestRoot 'Unknown/VoxRestore'
$null=New-Item -ItemType Directory -Path $unknown -Force
'keep me' | Set-Content -LiteralPath (Join-Path $unknown 'sentinel.txt')
Expect-Failure {& $installer -PackageRoot $PackageRoot -InstallRoot $unknown -Vst3Root (Join-Path $TestRoot 'UnknownVST3') -NoShortcuts -NoRegistration}
Assert-True ((Get-Content -LiteralPath (Join-Path $unknown 'sentinel.txt')) -eq 'keep me') 'Existing data was changed.'
$lockedFile=Join-Path $vstRoot 'VoxRestore.vst3/Contents/x86_64-win/VoxRestore.vst3'
$handle=[IO.File]::Open($lockedFile,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try {Expect-Failure {& $installer -PackageRoot $PackageRoot -InstallRoot $install -Vst3Root $vstRoot -NoShortcuts -NoRegistration}} finally {$handle.Dispose()}
# Inject a failure between the app and VST3 swaps to exercise real rollback.
$global:VoxRestoreTestFailVstSwap=$true
function Move-Item {
    param([string]$LiteralPath,[string]$Destination)
    if ($global:VoxRestoreTestFailVstSwap -and $LiteralPath -like '*.stage-*' -and $Destination -like '*VoxRestore.vst3') { $global:VoxRestoreTestFailVstSwap=$false;throw 'Simulated VST3 swap failure.' }
    Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination
}
try {Expect-Failure {& $installer -PackageRoot $PackageRoot -InstallRoot $install -Vst3Root $vstRoot -NoShortcuts -NoRegistration}} finally {Remove-Item Function:\Move-Item;Remove-Variable VoxRestoreTestFailVstSwap -Scope Global}
Assert-True ((Get-Content -LiteralPath $marker -Raw) -ceq $originalMarker) 'Rollback did not restore installation metadata.'
Assert-True (Test-Path -LiteralPath $lockedFile) 'Rollback did not restore the VST3.'
$tampered=Join-Path $TestRoot 'tampered-package'
$null=New-Item -ItemType Directory -Path $tampered
foreach ($name in @('Standalone','VST3','release-manifest.json')) { Copy-Item -LiteralPath (Join-Path $PackageRoot $name) -Destination $tampered -Recurse }
[IO.File]::AppendAllText((Join-Path $tampered 'Standalone/VoxRestore.exe'),'tampered')
Expect-Failure {Get-VerifiedPackage $tampered}
& $installer -PackageRoot $PackageRoot -InstallRoot $install -Vst3Root $vstRoot -NoShortcuts -NoRegistration
$updated=Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json
Assert-True (Test-Path -LiteralPath $updated.previousApp) 'Previous application version was not retained.'
Assert-True (Test-Path -LiteralPath $updated.previousVst) 'Previous VST3 version was not retained.'
Assert-True (-not $updated.previousVst.StartsWith($vstRoot+'\',[StringComparison]::OrdinalIgnoreCase)) 'VST3 backup is inside the plug-in scan directory.'
& (Join-Path $PSScriptRoot 'Uninstall-VoxRestore.ps1') -InstallRoot $install -NoShortcuts -NoRegistration
Assert-True (-not (Test-Path -LiteralPath $install)) 'Uninstall left the current app installed.'
Assert-True (-not (Test-Path -LiteralPath $updated.previousApp)) 'Uninstall left the owned backup.'
Assert-True (Test-Path -LiteralPath (Join-Path $unknown 'sentinel.txt')) 'Uninstall touched unrelated data.'
@{passed=$true;checks=$checks} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $TestRoot 'updates-tests.json') -Encoding UTF8
Write-Output ("Installer and updater policy checks passed: $checks")
& (Join-Path $PSScriptRoot 'Test-UpdateProgress.ps1') -TestRoot (Join-Path $TestRoot 'progress')
