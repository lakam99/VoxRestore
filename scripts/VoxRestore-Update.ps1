param(
    [ValidateSet('Check','Prepare')][string]$Mode = 'Check',
    [Parameter(Mandatory=$true)][string]$CurrentVersion,
    [string]$Report
)
. (Join-Path $PSScriptRoot 'Release-Common.ps1')
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
function Write-UpdateProgress([int]$Phase,[double]$Fraction=-1,[long]$Received=0,[long]$Total=0) {
    if(-not $Report){return}
    $path=$Report+'.progress.json';$temporary=$path+'.tmp'
    try {
        $json=@{phase=$Phase;fraction=$Fraction;received=$Received;total=$Total}|ConvertTo-Json -Compress
        [IO.File]::WriteAllText($temporary,$json,[Text.UTF8Encoding]::new($false))
        if([IO.File]::Exists($path)){
            $backup=$path+'.previous'
            [IO.File]::Replace($temporary,$path,$backup)
            if([IO.File]::Exists($backup)){[IO.File]::Delete($backup)}
        }else{[IO.File]::Move($temporary,$path)}
    } catch {
        # Progress is advisory. A short-lived UI read lock must not abort a
        # verified download or weaken any of the package checks below.
        try{if([IO.File]::Exists($temporary)){[IO.File]::Delete($temporary)}}catch{}
    }
}
function Receive-UpdateArchive([string]$Uri,[string]$Destination,[long]$ExpectedSize,[string]$UserAgent) {
    $request=[Net.HttpWebRequest]::Create($Uri)
    $request.UserAgent=$UserAgent;$request.Timeout=20000;$request.ReadWriteTimeout=15000
    $response=$null;$input=$null;$output=$null;$timer=[Diagnostics.Stopwatch]::StartNew();$lastReport=-1000L;$received=0L
    try {
        $response=$request.GetResponse();$input=$response.GetResponseStream()
        $output=[IO.File]::Open($Destination,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
        $buffer=New-Object byte[] 65536
        Write-UpdateProgress 1 0 0 $ExpectedSize
        while(($count=$input.Read($buffer,0,$buffer.Length)) -gt 0){
            if($timer.Elapsed.TotalSeconds -gt 240){throw 'The update download timed out.'}
            $received+=$count;if($received -gt $ExpectedSize){throw 'The download exceeded its declared size.'}
            $output.Write($buffer,0,$count)
            if($timer.ElapsedMilliseconds-$lastReport -ge 100){Write-UpdateProgress 1 ([double]$received/$ExpectedSize) $received $ExpectedSize;$lastReport=$timer.ElapsedMilliseconds}
        }
        if($received -ne $ExpectedSize){throw 'The download ended before the declared package size.'}
        Write-UpdateProgress 1 1 $received $ExpectedSize
    } finally {
        if($output){$output.Dispose()};if($input){$input.Dispose()};if($response){$response.Dispose()}
    }
}
try {
    Write-UpdateProgress 0
    $headers = @{ 'User-Agent'=('VoxRestore/' + $CurrentVersion); Accept='application/vnd.github+json'; 'X-GitHub-Api-Version'='2022-11-28' }
    $response = Invoke-WebRequest -Uri 'https://api.github.com/repos/lakam99/VoxRestore/releases/latest' -Headers $headers -UseBasicParsing -TimeoutSec 20
    if ($response.Content.Length -gt 2097152) { throw 'Release metadata is too large.' }
    $release = Get-VoxRestoreRelease ($response.Content | ConvertFrom-Json) $CurrentVersion
    $result = [ordered]@{ passed=$true; available=$release.available; version=$release.version; releaseUrl=$release.releaseUrl }
    if ($Mode -eq 'Prepare' -and $release.available) {
        $cache = Join-Path $env:LOCALAPPDATA ('VoxRestore/Updates/' + $release.version + '-' + [guid]::NewGuid().ToString('N'))
        $null = New-Item -ItemType Directory -Path $cache -Force
        $archive = Join-Path $cache $release.name
        $sums = Invoke-WebRequest -Uri $release.sums.browser_download_url -Headers @{'User-Agent'=$headers['User-Agent']} -UseBasicParsing -TimeoutSec 20
        $expected = Get-ReleaseChecksum $sums.Content $release.name $release.asset.digest
        Receive-UpdateArchive $release.asset.browser_download_url $archive $release.asset.size $headers['User-Agent']
        Write-UpdateProgress 2
        if ((Get-Item -LiteralPath $archive).Length -ne $release.asset.size -or (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ine $expected) { throw 'Downloaded package failed SHA-256 or size verification.' }
        $expanded = Join-Path $cache 'expanded'
        Write-UpdateProgress 3
        $null = New-Item -ItemType Directory -Path $expanded
        Expand-VerifiedArchive $archive $expanded
        $package = Join-Path $expanded 'VoxRestore'
        Write-UpdateProgress 4
        $manifest = Get-VerifiedPackage $package
        if ($manifest.version -cne $release.version) { throw 'Package version does not match the release tag.' }
        # Run the existing local installer from outside the directory it replaces.
        $bootstrap = Join-Path $cache 'bootstrap'
        $null = New-Item -ItemType Directory -Path $bootstrap
        foreach ($name in @('Install-VoxRestore.ps1','Release-Common.ps1')) {
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $bootstrap $name)
        }
        $result.packageRoot = $package
        $result.installer = Join-Path $bootstrap 'Install-VoxRestore.ps1'
        Write-UpdateProgress 5 1
    }
    if ($Report) { $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Report -Encoding UTF8 }
    else { $result | ConvertTo-Json -Depth 8 }
} catch {
    $failure = [ordered]@{ passed=$false; error=$_.Exception.Message }
    if ($Report) { $failure | ConvertTo-Json | Set-Content -LiteralPath $Report -Encoding UTF8 }
    else { $failure | ConvertTo-Json }
    exit 1
}
