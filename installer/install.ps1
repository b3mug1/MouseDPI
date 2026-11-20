# MouseControl - User-Mode Windows Installer
[CmdletBinding()]
param(
    [switch]$CreateDesktopShortcut,
    [switch]$StartAfterInstall
)

$ErrorActionPreference = "Stop"

$AppName = "MouseControl"
$AppVersion = "1.0.0"
$Publisher = "MouseControl Project"
$InstallDir = Join-Path $env:LOCALAPPDATA $AppName
$SourceDir = Split-Path -Parent $PSScriptRoot

# Locate built binary directory or package directory
$CandidateDirs = @(
    Join-Path $SourceDir "build\Release",
    Join-Path $PSScriptRoot "bin",
    $SourceDir
)

$BinDir = $null
foreach ($dir in $CandidateDirs) {
    if (Test-Path (Join-Path $dir "MouseControl.exe")) {
        $BinDir = $dir
        break
    }
}

if (-not $BinDir) {
    Write-Error "Could not locate MouseControl.exe. Please build the project in Release mode first."
    exit 1
}

Write-Host "Installing $AppName v$AppVersion..." -ForegroundColor Cyan

# Stop running instances
$running = Get-Process -Name $AppName -ErrorAction SilentlyContinue
if ($running) {
    Write-Host "Stopping running $AppName process..." -ForegroundColor Yellow
    $running | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}

# Create installation directory
if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}

# Copy binary payload
$PayloadFiles = @(
    "MouseControl.exe",
    "device_inspector.exe",
    "fmt.dll",
    "spdlog.dll",
    "sqlite3.dll"
)

foreach ($file in $PayloadFiles) {
    $src = Join-Path $BinDir $file
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $InstallDir -Force
        Write-Host "  -> Installed $file" -ForegroundColor Gray
    }
}

# Copy uninstaller script to target
$UninstallerTarget = Join-Path $InstallDir "uninstall.ps1"
$UninstallerSource = Join-Path $PSScriptRoot "uninstall.ps1"
if (Test-Path $UninstallerSource) {
    Copy-Item -Path $UninstallerSource -Destination $UninstallerTarget -Force
}

# Create Shell Shortcuts
$WshShell = New-Object -ComObject WScript.Shell
$ExePath = Join-Path $InstallDir "MouseControl.exe"

# Start Menu Shortcut
$StartMenuDir = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
$StartShortcutPath = Join-Path $StartMenuDir "$AppName.lnk"
$Shortcut = $WshShell.CreateShortcut($StartShortcutPath)
$Shortcut.TargetPath = $ExePath
$Shortcut.WorkingDirectory = $InstallDir
$Shortcut.Description = "Universal Gaming Mouse Management Software"
$Shortcut.Save()
Write-Host "  -> Created Start Menu shortcut" -ForegroundColor Gray

# Desktop Shortcut
if ($CreateDesktopShortcut) {
    $DesktopDir = [Environment]::GetFolderPath("Desktop")
    $DesktopShortcutPath = Join-Path $DesktopDir "$AppName.lnk"
    $DesktopShortcut = $WshShell.CreateShortcut($DesktopShortcutPath)
    $DesktopShortcut.TargetPath = $ExePath
    $DesktopShortcut.WorkingDirectory = $InstallDir
    $DesktopShortcut.Description = "Universal Gaming Mouse Management Software"
    $DesktopShortcut.Save()
    Write-Host "  -> Created Desktop shortcut" -ForegroundColor Gray
}

# Register Windows Uninstall Key (Settings -> Apps)
$UninstallRegPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$AppName"
if (-not (Test-Path $UninstallRegPath)) {
    New-Item -Path $UninstallRegPath -Force | Out-Null
}

$UninstallCommand = "powershell.exe -ExecutionPolicy Bypass -File `"$UninstallerTarget`""

Set-ItemProperty -Path $UninstallRegPath -Name "DisplayName" -Value $AppName
Set-ItemProperty -Path $UninstallRegPath -Name "DisplayVersion" -Value $AppVersion
Set-ItemProperty -Path $UninstallRegPath -Name "Publisher" -Value $Publisher
Set-ItemProperty -Path $UninstallRegPath -Name "InstallLocation" -Value $InstallDir
Set-ItemProperty -Path $UninstallRegPath -Name "DisplayIcon" -Value "$ExePath,0"
Set-ItemProperty -Path $UninstallRegPath -Name "UninstallString" -Value $UninstallCommand
Set-ItemProperty -Path $UninstallRegPath -Name "QuietUninstallString" -Value "$UninstallCommand -Quiet"
Set-ItemProperty -Path $UninstallRegPath -Name "NoModify" -Value 1 -Type DWord
Set-ItemProperty -Path $UninstallRegPath -Name "NoRepair" -Value 1 -Type DWord

Write-Host "$AppName v$AppVersion successfully installed to $InstallDir!" -ForegroundColor Green

if ($StartAfterInstall) {
    Write-Host "Starting $AppName..." -ForegroundColor Cyan
    Start-Process -FilePath $ExePath
}
