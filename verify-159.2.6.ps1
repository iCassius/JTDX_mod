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
  'bin\msys-hamlib-4.dll'
)
foreach ($relative in $required) {
  $path = Join-Path -Path $root -ChildPath $relative
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
    throw "Missing runtime file: $relative"
  }
}
$hamlib = Join-Path -Path $root -ChildPath 'bin\msys-hamlib-4.dll'
$hash = Get-Sha256 -Path $hamlib
Write-Output "root=$root"
Write-Output "hamlib_sha256=$hash"
if ($hash -ne '630A90E02F56E0D5F02A77E8D172F61F041399900DBFFA57A4FB989896895DB3') {
  throw 'Hamlib DLL SHA256 mismatch'
}
Write-Output 'runtime_check=PASS'
