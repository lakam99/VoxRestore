$ErrorActionPreference = 'Stop'
$script:VoxRestoreProduct = 'audio.arkam.voxrestore'
$script:VoxRestoreRepository = 'lakam99/VoxRestore'

function Get-StableVersion([string]$Text) {
    if ($Text -notmatch '^v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { throw 'Expected a stable major.minor.patch version.' }
    return [version]($Text.TrimStart('v'))
}
function Assert-ChildPath([string]$Path, [string]$Parent) {
    $absolute = [IO.Path]::GetFullPath($Path)
    $prefix = [IO.Path]::GetFullPath($Parent).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $absolute.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Path escapes its intended directory.' }
    return $absolute
}
function Assert-OwnedDirectory([string]$Path, [string]$Marker) {
    if (Test-Path -LiteralPath $Path) {
        $item = Get-Item -LiteralPath $Path -Force
        if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Installation target must be a regular directory.' }
        $file = Join-Path $Path $Marker
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw 'Refusing to replace a directory not owned by VoxRestore.' }
        $owner = Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
        if ($owner.product -ne $script:VoxRestoreProduct -or $owner.repository -ne $script:VoxRestoreRepository) { throw 'Installation ownership marker does not match.' }
        $pending = New-Object 'Collections.Generic.Queue[string]'
        $pending.Enqueue($item.FullName)
        while ($pending.Count -gt 0) {
            foreach ($child in Get-ChildItem -LiteralPath $pending.Dequeue() -Force) {
                if ($child.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Owned installation directories must not contain filesystem links.' }
                if ($child.PSIsContainer) { $pending.Enqueue($child.FullName) }
            }
        }
    }
}
function Assert-Unlocked([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        try { $handle = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None); $handle.Dispose() }
        catch { throw 'Close VoxRestore and any DAW using its VST3 before installing.' }
    }
}
function Get-VoxRestoreRelease($Release, [string]$CurrentVersion) {
    if ($Release.draft -or $Release.prerelease) { throw 'Automatic updates accept stable published releases only.' }
    $version = Get-StableVersion $Release.tag_name
    $current = Get-StableVersion $CurrentVersion
    $name = 'VoxRestore-' + $version.ToString(3) + '-Windows-x64.zip'
    $packageAssets = @($Release.assets | Where-Object { $_.name -eq $name -and $_.state -eq 'uploaded' })
    $checksumAssets = @($Release.assets | Where-Object { $_.name -eq 'SHA256SUMS.txt' -and $_.state -eq 'uploaded' })
    if ($packageAssets.Count -ne 1 -or $checksumAssets.Count -ne 1) { throw 'Release is missing its unique Windows package or checksum manifest.' }
    $prefix = 'https://github.com/lakam99/VoxRestore/releases/download/' + $Release.tag_name + '/'
    foreach ($asset in @($packageAssets[0], $checksumAssets[0])) {
        if ($asset.browser_download_url -cne ($prefix + $asset.name)) { throw 'Update URL does not belong to the expected repository and tag.' }
    }
    if ($packageAssets[0].size -lt 1 -or $packageAssets[0].size -gt 536870912 -or $checksumAssets[0].size -gt 65536) { throw 'Release asset size is outside the allowed range.' }
    if ($packageAssets[0].digest -notmatch '^sha256:[0-9a-fA-F]{64}$') { throw 'Release package has no GitHub SHA-256 digest.' }
    return [pscustomobject]@{ available = ($version -gt $current); version = $version.ToString(3); name = $name; asset = $packageAssets[0]; sums = $checksumAssets[0]; releaseUrl = 'https://github.com/lakam99/VoxRestore/releases/tag/' + $Release.tag_name }
}
function Expand-VerifiedArchive([string]$Archive, [string]$Destination) {
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        $total = [long]0
        $seen = @{}
        if ($zip.Entries.Count -gt 20000) { throw 'Too many archive entries.' }
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('\','/')
            if (-not $name.StartsWith('VoxRestore/', [StringComparison]::Ordinal) -or $name -match '(^|/)\.\.?(/|$)|:|[\x00-\x1f]' -or (($entry.ExternalAttributes -shr 16) -band 61440) -eq 40960) { throw 'Unsafe archive entry.' }
            $path = Assert-ChildPath (Join-Path $Destination $name) $Destination
            if ($seen.ContainsKey($path)) { throw 'Duplicate archive destination.' }
            $seen[$path] = $true
            $total += $entry.Length
            if ($total -gt 1610612736) { throw 'Archive expands beyond the size limit.' }
        }
        [IO.Compression.ZipFile]::ExtractToDirectory($Archive, $Destination)
    } finally { $zip.Dispose() }
}
function Get-ReleaseChecksum([object]$Content, [string]$Name, [string]$Digest) {
    if ($Content.Length -gt 65536) { throw 'Checksum manifest is too large.' }
    if ($Content -is [byte[]]) { $text=[Text.Encoding]::UTF8.GetString($Content) } else {$text=[string]$Content}
    $text=$text.TrimStart([char]0xFEFF)
    $matching=@($text -split '\r?\n' | Where-Object {$_ -match ('^[0-9a-fA-F]{64}\s+\*?'+[regex]::Escape($Name)+'$')})
    if ($matching.Count -ne 1) { throw 'Release checksum is missing or ambiguous.' }
    $expected=($matching[0] -split '\s+')[0].ToLowerInvariant()
    if (('sha256:'+$expected) -cne $Digest.ToLowerInvariant()) { throw 'GitHub digest and checksum manifest disagree.' }
    return $expected
}
function Get-VerifiedPackage([string]$PackageRoot) {
    $base = [IO.Path]::GetFullPath($PackageRoot)
    $manifest = Get-Content -LiteralPath (Join-Path $base 'release-manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.schema -ne 1 -or $manifest.product -ne $script:VoxRestoreProduct -or $manifest.repository -ne $script:VoxRestoreRepository -or $manifest.platform -ne 'windows-x64') { throw 'Package identity is invalid.' }
    $null = Get-StableVersion $manifest.version
    $seen = @{}
    foreach ($entry in $manifest.files.PSObject.Properties) {
        $name = $entry.Name
        if ($name -notmatch '^(Standalone/|VST3/VoxRestore\.vst3/)' -or $name -match '(^|/)\.\.?(/|$)|[\\:]' -or $entry.Value -notmatch '^[0-9a-fA-F]{64}$') { throw 'Invalid package manifest entry.' }
        $path = Assert-ChildPath (Join-Path $base $name) $base
        if ($seen.ContainsKey($path)) { throw 'Duplicate payload path.' }
        $seen[$path] = $true
        if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $entry.Value) { throw ('Package checksum mismatch: ' + $name) }
    }
    foreach ($required in @('Standalone/VoxRestore.exe','Standalone/onnxruntime.dll','Standalone/onnxruntime_providers_shared.dll','Standalone/VoxRestore-Update.ps1','Standalone/Install-VoxRestore.ps1','Standalone/Release-Common.ps1','Standalone/Uninstall-VoxRestore.ps1','VST3/VoxRestore.vst3/Contents/x86_64-win/VoxRestore.vst3','VST3/VoxRestore.vst3/Contents/x86_64-win/onnxruntime.dll','VST3/VoxRestore.vst3/Contents/x86_64-win/onnxruntime_providers_shared.dll')) {
        if (-not $seen.ContainsKey((Assert-ChildPath (Join-Path $base $required) $base))) { throw ('Required payload is missing: ' + $required) }
    }
    foreach ($folder in @('Standalone','VST3')) {
        foreach ($item in Get-ChildItem -LiteralPath (Join-Path $base $folder) -Recurse -Force) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Payload links are not permitted.' }
            if (-not $item.PSIsContainer -and -not $seen.ContainsKey($item.FullName)) { throw 'Unmanifested payload file.' }
        }
    }
    return $manifest
}
