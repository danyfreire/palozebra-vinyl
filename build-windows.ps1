$ErrorActionPreference = "Stop"

Write-Host "Palozebra // VINYL - Windows build" -ForegroundColor Cyan

function Assert-NativeSuccess([string]$Step) {
    if ($LASTEXITCODE -ne 0) {
        throw "$Step fallo con codigo de salida $LASTEXITCODE."
    }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake no esta instalado o no esta en PATH."
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git no esta instalado o no esta en PATH."
}

if (-not (Test-Path "$PSScriptRoot\JUCE\CMakeLists.txt")) {
    Write-Host "Descargando JUCE 8.0.8..."
    git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git "$PSScriptRoot\JUCE"
    Assert-NativeSuccess "La descarga de JUCE"
}

Push-Location $PSScriptRoot
try {
    Write-Host "Configurando CMake..." -ForegroundColor DarkCyan
    cmake -S . -B build -A x64
    Assert-NativeSuccess "La configuracion de CMake"

    Write-Host "Compilando smoke test..." -ForegroundColor DarkCyan
    cmake --build build --config Release --target scratch_engine_test
    Assert-NativeSuccess "La compilacion del smoke test"

    $testExe = Get-ChildItem -Path "$PSScriptRoot\build" -Recurse -Filter "scratch_engine_test.exe" | Select-Object -First 1
    if (-not $testExe) {
        throw "No se encontro scratch_engine_test.exe despues de compilar."
    }
    & $testExe.FullName
    Assert-NativeSuccess "El smoke test de ScratchEngine"

    Write-Host "Compilando VST3..." -ForegroundColor DarkCyan
    cmake --build build --config Release --target PalozebraVinyl_VST3
    Assert-NativeSuccess "La compilacion VST3"

    $artifact = Get-ChildItem -Path "$PSScriptRoot\build" -Recurse -Filter "Palozebra Vinyl.vst3" | Select-Object -First 1
    if ($artifact) {
        Write-Host "`nLISTO:" -ForegroundColor Green
        Write-Host $artifact.FullName
        Write-Host "`nEn Ableton: Settings > Plug-Ins > Rescan despues de copiarlo a tu carpeta VST3."
    } else {
        throw "La compilacion termino sin errores, pero no se encontro el bundle Palozebra Vinyl.vst3."
    }
}
finally {
    Pop-Location
}
