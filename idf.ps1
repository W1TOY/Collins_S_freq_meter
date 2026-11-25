<#
  Run ESP-IDF's idf.py on the fmaster firmware from the top-level folder.

  Usage (from anywhere):
    .\idf.ps1 build
    .\idf.ps1 flash monitor
    .\idf.ps1 -Port COM5 flash
    .\idf.ps1 menuconfig
    .\idf.ps1 fullclean

  The ESP-IDF environment is loaded automatically (export.ps1), so no
  "ESP-IDF terminal" is needed. It builds the top-level CMakeLists.txt (which
  pulls in fmaster/main and fmaster/sdkconfig) into fmaster\build, the same
  project and directory the ESP-IDF VS Code extension buttons use.
#>
param(
    [string]$Port = $(if ($env:ESPPORT) { $env:ESPPORT } else { 'COM3' }),
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$IdfArgs
)

$ErrorActionPreference = 'Stop'

$IdfPath    = if ($env:IDF_PATH_OVERRIDE) { $env:IDF_PATH_OVERRIDE } else { 'C:\Users\email\esp\v5.4\esp-idf' }
$ProjectDir = Join-Path $PSScriptRoot 'fmaster'

if (-not $IdfArgs -or $IdfArgs.Count -eq 0) { $IdfArgs = @('build') }

# Load the ESP-IDF environment once per shell session.
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue) -or $env:IDF_PATH -ne $IdfPath) {
    $env:IDF_PATH       = $IdfPath
    $env:IDF_TOOLS_PATH = 'C:\Users\email\.espressif'
    $env:IDF_PYTHON_ENV_PATH = Join-Path $env:IDF_TOOLS_PATH 'python_env\idf5.4_py3.11_env'
    # export.ps1 needs a Python on PATH: use the one the ESP-IDF installer shipped.
    $env:PATH = (Join-Path $env:IDF_TOOLS_PATH 'tools\idf-python\3.11.2') + ';' + $env:PATH
    Write-Host "Loading ESP-IDF environment from $IdfPath ..."
    $exportScript = Join-Path $IdfPath 'export.ps1'
    # export.ps1 prints progress on stderr; don't let PowerShell 5.1 treat that as fatal.
    $ErrorActionPreference = 'Continue'
    . $exportScript *> $null
    $ErrorActionPreference = 'Stop'
    if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
        throw "ESP-IDF environment failed to load (idf.py not found). Run $exportScript manually to see why."
    }
}

$env:IDF_TARGET = 'esp32'

# Only pass the serial port to commands that talk to the board.
$needsPort = $IdfArgs | Where-Object { $_ -match '^(flash|app-flash|monitor|erase[-_]flash|erase-otadata|read-otadata)$' }
$portArgs  = if ($needsPort) { @('-p', $Port) } else { @() }

# Same project (top-level CMakeLists.txt) and build dir as the ESP-IDF extension buttons.
idf.py -C $PSScriptRoot -B (Join-Path $ProjectDir 'build') @portArgs @IdfArgs
exit $LASTEXITCODE
