param([string]$Generator = 'Visual Studio 17 2022')
$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'build'
if ($Generator -eq 'Ninja') {
    & cmake -S $PSScriptRoot -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release
} else {
    & cmake -S $PSScriptRoot -B $build -G $Generator -A x64
}
if ($LASTEXITCODE -ne 0) {throw 'CMake configuration failed.'}
& cmake --build $build --config Release --target VoxRestore_VST3 VoxRestoreApp --parallel 6
if ($LASTEXITCODE -ne 0) {throw 'Build failed.'}
