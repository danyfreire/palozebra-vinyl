$ErrorActionPreference = "Stop"

Write-Host "Palozebra // VINYL - Windows build" -ForegroundColor Cyan

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake no está instalado o no está en PATH."
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git no está instalado o no está en PATH."
}

if (-not (Test-Path "$PSScriptRoot\JUCE\CMakeLists.txt")) {
    Write-Host "Descargando JUCE 8.0.8..."
    git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git "$PSScriptRoot\JUCE"
}

Push-Location $PSScriptRoot
try {
    cmake -S . -B build -A x64
    cmake --build build --config Release --target PalozebraVinyl_VST3

    $artifact = Get-ChildItem -Path "$PSScriptRoot\build" -Recurse -Filter "Palozebra Vinyl.vst3" | Select-Object -First 1
    if ($artifact) {
        Write-Host "`nLISTO:" -ForegroundColor Green
        Write-Host $artifact.FullName
        Write-Host "`nEn Ableton: Settings > Plug-Ins > Rescan después de copiarlo a tu carpeta VST3."
    } else {
        Write-Warning "La compilación terminó, pero no encontré automáticamente el bundle .vst3. Revisa build\PalozebraVinyl_artefacts."
    }
}
finally {
    Pop-Location
}
