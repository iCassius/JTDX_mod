$ErrorActionPreference = 'Stop'

function Get-Sha256 {
  param ([string] $Path)

  $algorithm = [System.Security.Cryptography.SHA256]::Create()
  try {
    $stream = [System.IO.File]::OpenRead($Path)
    try {
      $bytes = $algorithm.ComputeHash($stream)
      return (($bytes | ForEach-Object { $_.ToString('x2') }) -join '').ToUpperInvariant()
    }
    finally {
      $stream.Dispose()
    }
  }
  finally {
    $algorithm.Dispose()
  }
}

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$required = @(
  'bin\jtdx.exe',
  'bin\jtdxjt9.exe',
  'bin\msys-hamlib-4.dll',
  'bin\libfftw3f-3.dll',
  'bin\libfftw3f_threads-3.dll',
  'bin\libgfortran-5.dll',
  'bin\libgcc_s_seh-1.dll',
  'bin\libstdc++-6.dll',
  'bin\libwinpthread-1.dll',
  'bin\libusb-1.0.dll',
  'bin\Qt5Core.dll',
  'bin\Qt5Gui.dll',
  'bin\Qt5Multimedia.dll',
  'bin\Qt5Network.dll',
  'bin\Qt5SerialPort.dll',
  'bin\Qt5WebSockets.dll',
  'bin\Qt5Widgets.dll',
  'bin\qt.conf',
  'plugins\platforms\qwindows.dll',
  'plugins\audio\qtaudio_windows.dll'
)
foreach ($relative in $required) {
  $path = Join-Path -Path $root -ChildPath $relative
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
    throw "Missing runtime file: $relative"
  }
}

$forbiddenRuntimeNames = @(
  'Qt5AxContainer.dll',
  'Qt5AxBase.dll',
  'OmniRig.exe',
  'OmniRigTransceiver.dll',
  'OmniRigTransceiver.exe'
)
foreach ($forbiddenName in $forbiddenRuntimeNames) {
  $matches = Get-ChildItem -LiteralPath $root -Recurse -File -Filter $forbiddenName
  if ($matches.Count -gt 0) {
    throw "Hamlib-only package contains forbidden OmniRig/ActiveQt runtime file: $forbiddenName"
  }
}

$launcher = Join-Path -Path $root -ChildPath 'JTDX-159.2.7.cmd'
$launcherText = Get-Content -Raw -Encoding UTF8 $launcher
if ($launcherText -notmatch '"%~dp0bin\\jtdx\.exe"') {
  throw 'Launcher is not relative to its package directory'
}
if ($launcherText -match '[A-Za-z]:\\') {
  throw 'Launcher contains an absolute Windows path'
}

$jtdx = Join-Path -Path $root -ChildPath 'bin\jtdx.exe'
$versionInfo = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($jtdx)
if ($versionInfo.FileVersion -ne '2.2.159.2') {
  throw "Unexpected PE FileVersion: $($versionInfo.FileVersion)"
}
$readme = Join-Path -Path $root -ChildPath 'README_2.2.159.2.7-test_zh-CN.txt'
if (-not (Select-String -LiteralPath $readme -Pattern '2\.2\.159\.2\.7-test' -Quiet)) {
  throw 'Release README does not identify 2.2.159.2.7-test'
}

$hamlib = Join-Path -Path $root -ChildPath 'bin\msys-hamlib-4.dll'
$hash = Get-Sha256 -Path $hamlib
Write-Output "root=$root"
Write-Output "file_version=$($versionInfo.FileVersion)"
Write-Output "hamlib_sha256=$hash"
if ($hash -ne '630A90E02F56E0D5F02A77E8D172F61F041399900DBFFA57A4FB989896895DB3') {
  throw 'Hamlib DLL SHA256 mismatch'
}
Write-Output 'runtime_check=PASS'
