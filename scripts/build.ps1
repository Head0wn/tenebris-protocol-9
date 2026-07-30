[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDirectory = Join-Path $RepoRoot "build/windows"

if ($Clean -and (Test-Path $BuildDirectory)) {
    Remove-Item -Recurse -Force $BuildDirectory
}

cmake -S $RepoRoot -B $BuildDirectory `
    -DTENEBRIS_BUILD_TESTS=ON `
    -DTENEBRIS_WARNINGS_AS_ERRORS=ON

cmake --build $BuildDirectory --config $Configuration --parallel
ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure

Write-Host "TENEBRIS $Configuration build and tests completed successfully."
