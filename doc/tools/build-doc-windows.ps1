<#
.SYNOPSIS
  Build rsyslog docs on Windows with a local virtual environment.

.DESCRIPTION
  - Creates/uses .venv-docs under doc/
  - Installs requirements from doc/requirements.txt
  - Builds Sphinx docs (HTML by default) into doc/build

.PARAMETER Clean
  Remove previous build output before building

.PARAMETER Strict
  Treat Sphinx warnings as errors

.PARAMETER Format
  Output format (html or epub). Default: html

.PARAMETER Extra
  Additional options passed to sphinx-build
#>

[CmdletBinding()]
param(
  [switch]$Clean,
  [switch]$Strict,
  [ValidateSet('html','epub')]
  [string]$Format = 'html',
  [string]$Extra = ''
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DocDir    = Resolve-Path (Join-Path $ScriptDir '..')
$VenvDir   = Join-Path $DocDir '.venv-docs'
$ReqFile   = Join-Path $DocDir 'requirements.txt'
$OutputDir = Join-Path $DocDir 'build'

function Ensure-Python3 {
  if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Error 'python (Python 3) is required on PATH.'
    exit 1
  }
}

function New-Venv {
  # Prefer built-in venv
  $venvHelp = (& python -m venv -h) 2>$null
  if ($LASTEXITCODE -eq 0) {
    & python -m venv $VenvDir
  } else {
    # Fallback to virtualenv if needed
    & python -m pip show virtualenv *> $null
    if ($LASTEXITCODE -ne 0) {
      & python -m pip install --user virtualenv
    }
    & python -m virtualenv $VenvDir
  }
}

Ensure-Python3

if (-not (Test-Path $VenvDir)) {
  Write-Host "Creating virtual environment at $VenvDir"
  New-Venv
}

# Activate venv
$Activate = Join-Path $VenvDir 'Scripts/Activate.ps1'
. $Activate

python -m pip install --upgrade pip
python -m pip install -r $ReqFile

if ($Clean) { Remove-Item -Recurse -Force $OutputDir -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$opts = $Extra
if ($Strict) { $opts = "-W $opts" }

Write-Host "Building docs: format=$Format output=$OutputDir"
& sphinx-build $opts -b $Format (Join-Path $DocDir 'source') $OutputDir

Write-Host "Done. Open $($OutputDir)\index.html for HTML builds."

