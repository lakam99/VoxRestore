param(
    [string]$Gitleaks = 'gitleaks',
    [string[]]$Artifacts = @()
)
$ErrorActionPreference = 'Stop'
$scanner = (Get-Command $Gitleaks -ErrorAction Stop).Source
$repository = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$targets = @($repository) + $Artifacts
foreach ($target in $targets) {
    $resolved = (Resolve-Path -LiteralPath $target).Path
    $report = [System.IO.Path]::GetTempFileName()
    & $scanner dir $resolved --max-archive-depth 3 --ignore-gitleaks-allow --redact=100 --no-banner --report-format json --report-path $report
    if ($LASTEXITCODE -ne 0) {
        throw ('Publication secret scan failed. Redacted report: ' + $report)
    }
    Write-Host ('Secret scan passed. Redacted report: ' + $report)
}
