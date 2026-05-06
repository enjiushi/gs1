param(
    [string]$BuildDir = "build_godot",
    [string]$GodotSourceDir = "",
    [string]$GodotCppSourceDir = "",
    [switch]$BuildAdapter
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$defaultGodotSourceDir = Join-Path (Split-Path -Parent $repoRoot) "godot"
$defaultGodotCppSourceDir = Join-Path $repoRoot "third_party\godot-cpp"

if ([string]::IsNullOrWhiteSpace($GodotSourceDir)) {
    $GodotSourceDir = $defaultGodotSourceDir
}

if ([string]::IsNullOrWhiteSpace($GodotCppSourceDir)) {
    $GodotCppSourceDir = $defaultGodotCppSourceDir
}

$godotInterfaceHeader = Join-Path $GodotSourceDir "core\extension\gdextension_interface.gen.h"
$godotEditorBinary = Join-Path $GodotSourceDir "bin\godot.windows.editor.dev.x86_64.console.exe"
$godotCppCmakeLists = Join-Path $GodotCppSourceDir "CMakeLists.txt"

$missing = @()
if (-not (Test-Path $godotInterfaceHeader)) {
    $missing += "Godot source header missing: $godotInterfaceHeader"
}
if (-not (Test-Path $godotEditorBinary)) {
    $missing += "Godot editor binary missing: $godotEditorBinary"
}
if (-not (Test-Path $godotCppCmakeLists)) {
    $missing += "godot-cpp checkout missing: $godotCppCmakeLists"
}

if ($missing.Count -gt 0) {
    Write-Host "Godot adapter prerequisites are not ready." -ForegroundColor Yellow
    $missing | ForEach-Object { Write-Host " - $_" -ForegroundColor Yellow }
    Write-Host ""
    Write-Host "Expected happy-path layout:" -ForegroundColor Cyan
    Write-Host " - Repo:        $repoRoot"
    Write-Host " - Godot:       $defaultGodotSourceDir"
    Write-Host " - godot-cpp:   $defaultGodotCppSourceDir"
    Write-Host ""
    Write-Host "If third_party/godot-cpp is missing, run:" -ForegroundColor Cyan
    Write-Host " - git submodule update --init --recursive"
    exit 1
}

$configureArgs = @(
    "-S", $repoRoot,
    "-B", (Join-Path $repoRoot $BuildDir),
    "-DGS1_BUILD_GODOT_ADAPTER=ON",
    "-DGS1_GODOT_SOURCE_DIR=$($GodotSourceDir -replace '\\','/')",
    "-DGS1_GODOT_CPP_SOURCE_DIR=$($GodotCppSourceDir -replace '\\','/')",
    "-DGS1_GODOT_EDITOR_BINARY=$($godotEditorBinary -replace '\\','/')"
)

Write-Host "Configuring Godot-enabled build..." -ForegroundColor Cyan
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($BuildAdapter) {
    Write-Host "Building gs1_godot_adapter..." -ForegroundColor Cyan
    & cmake --build (Join-Path $repoRoot $BuildDir) --target gs1_godot_adapter --config Debug
    exit $LASTEXITCODE
}
