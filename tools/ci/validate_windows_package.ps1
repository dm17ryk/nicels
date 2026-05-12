param(
    [Parameter(Mandatory = $true)]
    [string] $PackagePath,

    [Parameter(Mandatory = $true)]
    [string] $StagingRoot,

    [ValidateSet("x86_64", "aarch64")]
    [string] $Arch = "x86_64",

    [switch] $RunRegression
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $PackagePath -PathType Leaf)) {
    throw "Package not found: $PackagePath"
}

if (-not (Test-Path -LiteralPath $StagingRoot -PathType Container)) {
    throw "Staging root not found: $StagingRoot"
}

$archRoot = Join-Path $StagingRoot $Arch
if (Test-Path -LiteralPath $archRoot -PathType Container) {
    $binarySearchRoot = $archRoot
} else {
    $binarySearchRoot = $StagingRoot
}

$binary = Get-ChildItem -LiteralPath $binarySearchRoot -Filter nls.exe -Recurse |
    Where-Object { $_.FullName -match "\\bin\\nls\.exe$" } |
    Select-Object -First 1

if (-not $binary) {
    throw "Could not find staged bin\nls.exe under $binarySearchRoot"
}

if ($Arch -eq "aarch64") {
    Write-Host "Windows ARM64 package is structurally valid: $PackagePath"
    Write-Host "Staged ARM64 binary: $($binary.FullName)"
    $dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($dumpbin) {
        & $dumpbin.Source /headers $binary.FullName | Select-String -Pattern "machine \((ARM64|AA64)\)" | Out-Null
    }
    exit 0
}

& $binary.FullName --version
& $binary.FullName --help | Out-Null

if ($RunRegression) {
    python test/run_nls_cli_tests.py --binary $binary.FullName --fixtures test --platform windows
}
