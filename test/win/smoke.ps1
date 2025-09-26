param(
    [string]$Preset = "windows-ucrt-release"
)

cmake --preset $Preset | Out-Null
cmake --build --preset $Preset --parallel | Out-Null
$bin = Join-Path "build" "windows-release/bin/Release/nicels.exe"
& $bin --long --report long @args
& $bin --tree --tree-depth 2
