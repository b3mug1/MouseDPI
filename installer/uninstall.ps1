# MouseControl - User-Mode Windows Uninstaller
[CmdletBinding()]
param(
    [switch]$Quiet
)

$AppName = "MouseControl"
$InstallDir = Join-Path $env:LOCALAPPDATA $AppName

if (-not $Quiet) {
    Write-Host "Uninstalling $AppName..." -ForegroundColor Yellow
}

# Stop running instances
$running = Get-Process -Name $AppName -ErrorAction SilentlyContinue
if ($running) {
    $running | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}

# Remove Startup Registry Entry
$StartupRegPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
if (Get-ItemProperty -Path $StartupRegPath -Name $AppName -ErrorAction SilentlyContinue) {
    Remove-ItemProperty -Path $StartupRegPath -Name $AppName -Force -ErrorAction SilentlyContinue
}

# Remove Uninstall Registry Entry
$UninstallRegPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$AppName"
if (Test-Path $UninstallRegPath) {
    Remove-Item -Path $UninstallRegPath -Recurse -Force -ErrorAction SilentlyContinue
}

# Remove Shortcuts
$StartShortcut = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\$AppName.lnk"
if (Test-Path $StartShortcut) {
    Remove-Item -Path $StartShortcut -Force -ErrorAction SilentlyContinue
}

$DesktopShortcut = Join-Path ([Environment]::GetFolderPath("Desktop")) "$AppName.lnk"
if (Test-Path $DesktopShortcut) {
    Remove-Item -Path $DesktopShortcut -Force -ErrorAction SilentlyContinue
}

# Remove Installed Files (schedule directory deletion if locked)
if (Test-Path $InstallDir) {
    Start-Sleep -Milliseconds 500
    Remove-Item -Path $InstallDir -Recurse -Force -ErrorAction SilentlyContinue
}

if (-not $Quiet) {
    Write-Host "$AppName has been cleanly uninstalled." -ForegroundColor Green
}
