@echo off
setlocal
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0verify-159.2.7.ps1"
exit /b %ERRORLEVEL%
