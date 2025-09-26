$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$app = Join-Path $root '..\nicels.exe'
$output = Join-Path $root 'smoke.actual'
$expected = Join-Path (Join-Path $root '..') 'expected\windows_smoke.txt'

try {
    & $app --no-icons --no-color --group-directories-first $root | Out-File -Encoding utf8 $output
    & $app --tree --tree-depth=2 $root | Out-File -Encoding utf8 -Append $output

    if (-not (Test-Path $expected)) {
        throw "Expected output not found: $expected"
    }

    $expectedContent = Get-Content -Path $expected
    $actualContent = Get-Content -Path $output
    $diff = Compare-Object -ReferenceObject $expectedContent -DifferenceObject $actualContent
    if ($diff) {
        $message = $diff | Format-Table | Out-String
        Write-Error -Message $message
        throw "Windows smoke test output differed. See $output"
    }

    Remove-Item $output
    Write-Host 'Windows smoke test passed.'
}
catch {
    if (Test-Path $output) {
        Write-Warning "Windows smoke test failed; kept $output for inspection."
    }
    throw
}
