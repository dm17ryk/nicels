$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir '..\..')
$exe = Join-Path $repoRoot 'nicels.exe'

if (-not (Test-Path $exe)) {
    Write-Error "nicels.exe not found at $exe. Build the project first."
}

Set-Location $scriptDir

Write-Host '# nicels long listing'
& $exe -l --no-color --no-icons .

Write-Host '\n# nicels git status (expected empty)'
& $exe --git-status --no-color --no-icons .
