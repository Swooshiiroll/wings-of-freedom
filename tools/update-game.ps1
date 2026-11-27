<#
.SYNOPSIS
    Installs or updates Wings of Freedom to the newest build on GitHub.

.DESCRIPTION
    Every test build is published as a GitHub release (pre-releases included). This fetches the
    newest one for Windows and installs it into a folder: by default the folder this script is in,
    which is the game's own folder once it has been installed (the script ships inside each build).
    The game's files are replaced; tuning.txt and anything else of yours is kept. Nothing is
    downloaded if the newest build is already installed.

    No GitHub account is needed: the repository is public.

.PARAMETER Dir
    Where to install. Defaults to the folder this script is in.

.PARAMETER Force
    Reinstall even if the newest build is already installed.

.PARAMETER Launch
    Start the game afterwards.

.EXAMPLE
    .\update-game.ps1
    .\update-game.ps1 -Dir "$env:USERPROFILE\Documents\Wings of Freedom" -Launch
#>
[CmdletBinding()]
param(
    [string]$Dir = $PSScriptRoot,
    [switch]$Force,
    [switch]$Launch
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue' # Invoke-WebRequest is far faster without the progress bar
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$Repo = 'Swooshiiroll/wings-of-freedom'
$Exe = 'WingsOfFreedom.exe'
$Stamp = 'installed-release.txt'

if (-not $Dir) { $Dir = (Get-Location).Path }
New-Item -ItemType Directory -Force -Path $Dir | Out-Null
$Dir = (Resolve-Path $Dir).Path

# The newest release, pre-releases included (the API lists them newest first).
Write-Host "Checking for the newest build of Wings of Freedom..."
$releases = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases?per_page=10" `
    -Headers @{ 'User-Agent' = 'wings-of-freedom-updater'; 'Accept' = 'application/vnd.github+json' }
$release = $releases | Where-Object { -not $_.draft } | Select-Object -First 1
if (-not $release) { throw "No releases found at https://github.com/$Repo/releases" }
$asset = $release.assets | Where-Object { $_.name -like '*-windows-x64.zip' } | Select-Object -First 1
if (-not $asset) { throw "Release $($release.tag_name) has no Windows build." }

$stampPath = Join-Path $Dir $Stamp
$installed = if (Test-Path $stampPath) { (Get-Content $stampPath -Raw).Trim() } else { '' }
if ($installed -eq $release.tag_name -and (Test-Path (Join-Path $Dir $Exe)) -and -not $Force) {
    Write-Host "Already up to date: $installed in $Dir"
} else {
    if (Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($Exe)) -ErrorAction SilentlyContinue) {
        throw "Wings of Freedom is running. Close it, then run this again."
    }
    $from = if ($installed) { "$installed -> " } else { '' }
    Write-Host "Installing $from$($release.tag_name) ($([math]::Round($asset.size / 1MB, 1)) MB) into $Dir"

    $work = Join-Path ([IO.Path]::GetTempPath()) ("wof-update-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $work | Out-Null
    try {
        $zip = Join-Path $work $asset.name
        Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zip -UseBasicParsing `
            -Headers @{ 'User-Agent' = 'wings-of-freedom-updater' }
        Expand-Archive -Path $zip -DestinationPath (Join-Path $work 'x') -Force
        # The build is one folder (wings-of-freedom\...); its contents go into the install folder.
        $top = Get-ChildItem (Join-Path $work 'x') -Directory | Select-Object -First 1
        $source = if ($top -and (Test-Path (Join-Path $top.FullName $Exe))) { $top.FullName } else { Join-Path $work 'x' }
        Copy-Item -Path (Join-Path $source '*') -Destination $Dir -Recurse -Force
        Set-Content -Path $stampPath -Value $release.tag_name -Encoding ascii
    } finally {
        Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
    }
    Write-Host "Done: $($release.tag_name)$(if ($release.prerelease) { ' (pre-release)' })"
    Write-Host "Notes: $($release.html_url)"
}

if ($Launch) {
    Start-Process -FilePath (Join-Path $Dir $Exe) -WorkingDirectory $Dir
}
