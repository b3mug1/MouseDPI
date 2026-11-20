@echo off
title MouseControl Setup
echo ========================================================
echo   MouseControl v1.0.0 - Windows Installer
echo ========================================================
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\install.ps1" -CreateDesktopShortcut -StartAfterInstall
echo.
pause
