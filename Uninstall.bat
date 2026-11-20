@echo off
title MouseControl Uninstaller
echo ========================================================
echo   MouseControl v1.0.0 - Windows Uninstaller
echo ========================================================
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\uninstall.ps1"
echo.
pause
