@echo off
setlocal
set "SCRIPT=%~dp0export-jtdx-diagnostics.ps1"
set "WINPS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%WINPS%" (
  "%WINPS%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%"
) else (
  where pwsh.exe >nul 2>nul
  if errorlevel 1 (
    echo Windows PowerShell 5.1 or pwsh.exe is required.
    pause
    exit /b 1
  )
  pwsh.exe -NoLogo -NoProfile -File "%SCRIPT%"
)
set "RESULT=%ERRORLEVEL%"
pause
exit /b %RESULT%
