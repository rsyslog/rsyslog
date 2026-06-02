param(
    [string] $Distro = "Ubuntu-24.04",
    [string] $RepoPath = "",
    [string] $AptMirror = "",
    [switch] $VerifyBuild,
    [switch] $AllowWindowsMount
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($RepoPath -eq "") {
    $RepoPath = (wsl.exe -d $Distro -- bash -lc 'printf "%s" "$HOME/rsyslog"')
    if ($LASTEXITCODE -ne 0 -or $RepoPath -eq "") {
        throw "Unable to determine the WSL home directory for distro '$Distro'. Pass -RepoPath explicitly."
    }
}

function Quote-Bash {
    param([string] $Value)
    return "'" + $Value.Replace("'", "'\''") + "'"
}

$setupArgs = @()
if ($AptMirror -ne "") {
    $setupArgs += "--apt-mirror"
    $setupArgs += $AptMirror
}
if ($VerifyBuild) {
    $setupArgs += "--verify-build"
}
if ($AllowWindowsMount) {
    $setupArgs += "--allow-windows-mount"
}

$quotedArgs = @()
foreach ($arg in $setupArgs) {
    $quotedArgs += (Quote-Bash $arg)
}

$repo = Quote-Bash $RepoPath
$argText = $quotedArgs -join " "
$command = "cd $repo && sudo ./devtools/codex-setup.sh $argText"

Write-Host "Running rsyslog Ubuntu 24.04 WSL setup in distro '$Distro'"
Write-Host "Repository: $RepoPath"
if ($AptMirror -ne "") {
    Write-Host "Apt mirror override: $AptMirror"
}
if ($VerifyBuild) {
    Write-Host "Build verification: enabled"
}

wsl.exe -d $Distro -- bash -lc $command
if ($LASTEXITCODE -ne 0) {
    throw "WSL setup failed with exit code $LASTEXITCODE"
}
