#Requires -Version 7.3

Push-Location (Join-Path $PSScriptRoot "..")
try {
    $ErrorActionPreference = 'Stop'
    $PSNativeCommandUseErrorActionPreference = $true
    foreach ($command in 'qmake', 'nmake', 'windeployqt', 'python') {
        if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
            throw "Missing $command. Use an x64 Visual Studio developer shell with Qt 6.8 on PATH."
        }
    }
    $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    if (-not (Test-Path $iscc)) { throw 'Inno Setup 6 is required.' }

    New-Item -ItemType Directory -Force build/standalone, build/windows-package | Out-Null
    Push-Location build/standalone
    try {
        qmake ../../read-aloud.pro CONFIG+=standalone
        nmake
    } finally {
        Pop-Location
    }
    $package = 'build/windows-package'
    Copy-Item build/standalone/release/bvos-read-aloud.exe $package/
    windeployqt --release --qmldir src "$package/bvos-read-aloud.exe"
    python -m venv build/windows-worker-venv
    $workerPython = 'build/windows-worker-venv/Scripts/python.exe'
    & $workerPython -m pip install -r packaging/worker-requirements.txt
    & $workerPython -m PyInstaller --noconfirm --onedir --name bvos-speech --collect-all espeakng_loader --collect-data certifi --collect-data kokoro_onnx speech/read-aloud.py
    & dist/bvos-speech/bvos-speech.exe --help
    New-Item -ItemType Directory -Force "$package/speech-worker" | Out-Null
    Copy-Item dist/bvos-speech/* "$package/speech-worker/" -Recurse -Force
    $env:BVOS_VERSION = (Get-Content manifest.json | ConvertFrom-Json).version
    & $iscc packaging/windows.iss
} finally {
    Pop-Location
}
