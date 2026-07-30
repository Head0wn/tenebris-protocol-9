[CmdletBinding()]
param(
    [Parameter()]
    [ValidateRange(1, 100)]
    [int]$Cycles = 10,

    [Parameter()]
    [string]$Executable = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($Executable)) {
    $candidates = @(
        (Join-Path $PSScriptRoot "..\build\game\Release\TENEBRIS.exe"),
        (Join-Path $PSScriptRoot "..\build\game\Debug\TENEBRIS.exe"),
        (Join-Path $PSScriptRoot "TENEBRIS.exe")
    )

    $Executable = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if ([string]::IsNullOrWhiteSpace($Executable) -or -not (Test-Path $Executable)) {
    throw "TENEBRIS.exe introuvable. Utilisez -Executable avec le chemin exact de la build."
}

$Executable = (Resolve-Path $Executable).Path
$startedAt = Get-Date

Write-Host "TENEBRIS — Validation Vulkan" -ForegroundColor Cyan
Write-Host "Executable : $Executable"
Write-Host "Cycles     : $Cycles"
Write-Host ""

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    Write-Host ("[{0}/{1}] Démarrage du smoke test GPU..." -f $cycle, $Cycles)

    $process = Start-Process \
        -FilePath $Executable \
        -ArgumentList "--gpu-smoke-test" \
        -NoNewWindow \
        -Wait \
        -PassThru

    if ($process.ExitCode -ne 0) {
        throw "Le cycle $cycle a échoué avec le code $($process.ExitCode)."
    }
}

$elapsed = (Get-Date) - $startedAt
Write-Host ""
Write-Host ("Validation réussie : {0} cycles en {1:n1} secondes." -f $Cycles, $elapsed.TotalSeconds) -ForegroundColor Green
