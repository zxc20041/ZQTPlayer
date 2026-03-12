param(
    [string]$WorkspaceFolder
)

$ErrorActionPreference = 'Stop'

$src = Join-Path $WorkspaceFolder 'build/vscode-Release'
$dst = Join-Path $WorkspaceFolder 'build/deploy'

# Clean and create deploy directory
if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
New-Item $dst -ItemType Directory | Out-Null

# Copy exe
Copy-Item "$src/ZQTPlayer.exe" $dst
Write-Host "Copied ZQTPlayer.exe"

# Copy RTX bridge DLL
if (Test-Path "$src/rtx_hdr_vsr_bridge.dll") {
    Copy-Item "$src/rtx_hdr_vsr_bridge.dll" $dst
    Write-Host "Copied rtx_hdr_vsr_bridge.dll"
}

# Copy FFmpeg DLLs
$ffbin = 'C:/ffmpeg/ffmpeg-master-latest-win64-lgpl-shared/bin'
if (Test-Path $ffbin) {
    Get-ChildItem $ffbin -Filter '*.dll' | ForEach-Object {
        Copy-Item $_.FullName $dst
        Write-Host "Copied $($_.Name)"
    }
}

# Run windeployqt
$windeployqt = 'C:/Qt/6.10.2/mingw_64/bin/windeployqt6.exe'
& $windeployqt --qmldir $WorkspaceFolder "$dst/ZQTPlayer.exe"

Write-Host "`nDeploy complete: $dst"
