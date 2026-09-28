param(
  [Parameter(Mandatory)]
  [ValidatePattern('^[A-Za-z0-9._-]+$')]
  [string] $CandidateId,

  [Parameter(Mandatory)]
  [ValidatePattern('^[A-Za-z0-9._-]+$')]
  [string] $PackageName,

  [ValidatePattern('^[A-Za-z0-9._-]+$')]
  [string] $StageDirectoryName = $CandidateId,

  [switch] $ValidateOnly
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$supportRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sourceRoot = (Resolve-Path (Join-Path $supportRoot '..')).Path
$workspaceRoot = (Split-Path -Parent $sourceRoot)
$stageRoot = Join-Path $supportRoot "staging\$StageDirectoryName"
$extractRoot = Join-Path $supportRoot "extract\$CandidateId"
$evidenceRoot = Join-Path $supportRoot "evidence\$CandidateId"
$zipPath = Join-Path $workspaceRoot "$PackageName.zip"
$legacySidecarPath = "$zipPath.sha256"
$manifestPath = Join-Path $evidenceRoot "manifest-$CandidateId.csv"
$verifyLogPath = Join-Path $evidenceRoot 'package-verify.log'

$resolvedWorkspaceRoot = [IO.Path]::GetFullPath($workspaceRoot)
$resolvedZipPath = [IO.Path]::GetFullPath($zipPath)
if (-not [String]::Equals([IO.Path]::GetDirectoryName($resolvedZipPath),
                           $resolvedWorkspaceRoot,
                           [StringComparison]::OrdinalIgnoreCase)) {
  throw "Release ZIP must be a direct child of $resolvedWorkspaceRoot"
}

$resolvedStage = (Resolve-Path -LiteralPath $stageRoot).Path
$allowedStagePrefix = (Join-Path $supportRoot 'staging') + [IO.Path]::DirectorySeparatorChar
if (-not $resolvedStage.StartsWith($allowedStagePrefix, [StringComparison]::OrdinalIgnoreCase)) {
  throw "Stage must remain inside $supportRoot\staging"
}

$requiredRoots = @('bin', 'plugins', 'share')
foreach ($entry in $requiredRoots) {
  if (-not (Test-Path -LiteralPath (Join-Path $resolvedStage $entry))) {
    throw "Required stage entry is missing: $entry"
  }
}
$stageRoots = @(Get-ChildItem -LiteralPath $resolvedStage -Force | ForEach-Object Name | Sort-Object)
if (Compare-Object ($stageRoots | Sort-Object) ($requiredRoots | Sort-Object)) {
  throw "Stage must contain exactly these top-level directories: $($requiredRoots -join ', ')"
}

function Get-TreeRows([string] $Root) {
  Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object {
    $_.FullName.Substring($Root.Length + 1).Replace('\', '/')
  } | ForEach-Object {
    [pscustomobject]@{
      path = $_.FullName.Substring($Root.Length + 1).Replace('\', '/')
      bytes = $_.Length
      sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
  }
}

function Test-ZipAgainstStage([string] $ArchivePath, [string] $RootPath) {
  $archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
  try {
    $entries = @($archive.Entries | Where-Object { $_.Name -ne '' })
    $paths = @($entries | ForEach-Object FullName)
    if (($paths | Sort-Object -Unique).Count -ne $paths.Count) { throw 'ZIP contains duplicate paths' }
    foreach ($path in $paths) {
      if ($path.Contains('\') -or $path.StartsWith('/') -or [IO.Path]::IsPathRooted($path) -or $path -match '^[A-Za-z]:') {
        throw "ZIP contains an absolute or non-normalized path: $path"
      }
      $segments = @($path -split '/')
      if ($segments.Count -lt 2 -or @($segments | Where-Object { $_ -in @('', '.', '..') }).Count -gt 0) {
        throw "ZIP contains a path traversal or invalid segment: $path"
      }
    }
    $roots = @($paths | ForEach-Object { ($_ -split '/')[0] } | Sort-Object -Unique)
    if (Compare-Object ($roots | Sort-Object) ($requiredRoots | Sort-Object)) {
      throw "Unexpected ZIP roots: $($roots -join ', ')"
    }

    $stageRows = @(Get-TreeRows $RootPath)
    if ($stageRows.Count -ne $entries.Count) { throw "ZIP/stage file count differs: $($entries.Count)/$($stageRows.Count)" }
    $entryMap = @{}
    foreach ($entry in $entries) { $entryMap[$entry.FullName] = $entry }
    foreach ($row in $stageRows) {
      if (-not $entryMap.ContainsKey($row.path)) { throw "ZIP entry missing: $($row.path)" }
      $entry = $entryMap[$row.path]
      if ($entry.Length -ne $row.bytes) { throw "ZIP length mismatch: $($row.path)" }
      $stream = $entry.Open()
      $sha = [Security.Cryptography.SHA256]::Create()
      try { $zipHash = [Convert]::ToHexString($sha.ComputeHash($stream)) }
      finally { $stream.Dispose(); $sha.Dispose() }
      if ($zipHash -cne $row.sha256) { throw "ZIP SHA-256 mismatch: $($row.path)" }
    }
    [pscustomobject]@{ Count = $entries.Count; Roots = $roots; Rows = $stageRows }
  }
  finally { $archive.Dispose() }
}

if ($ValidateOnly) {
  if (-not (Test-Path -LiteralPath $zipPath)) { throw "ZIP not found: $zipPath" }
  $actualZipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash
  $result = Test-ZipAgainstStage $zipPath $resolvedStage
  "VALIDATED=$($result.Count) files"
  "ROOTS=$($result.Roots -join ',')"
  "ZIP_SHA256=$actualZipHash"
  exit 0
}

if (Test-Path -LiteralPath $zipPath) { throw "Refusing to overwrite existing ZIP: $zipPath" }
if (Test-Path -LiteralPath $legacySidecarPath) {
  throw "Refusing to proceed because a same-name sidecar already exists: $legacySidecarPath"
}
if (Test-Path -LiteralPath $extractRoot) { throw "Refusing to overwrite existing extract directory: $extractRoot" }
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null

[IO.Compression.ZipFile]::CreateFromDirectory(
  $resolvedStage,
  $zipPath,
  [IO.Compression.CompressionLevel]::Optimal,
  $false
)
New-Item -ItemType Directory -Path $extractRoot | Out-Null
[IO.Compression.ZipFile]::ExtractToDirectory($zipPath, $extractRoot)
$result = Test-ZipAgainstStage $zipPath $resolvedStage
$extractRows = @(Get-TreeRows $extractRoot)
if ($extractRows.Count -ne $result.Rows.Count) { throw 'Extracted file count differs from the stage' }
for ($i = 0; $i -lt $result.Rows.Count; $i++) {
  if ($extractRows[$i].path -cne $result.Rows[$i].path -or
      $extractRows[$i].bytes -ne $result.Rows[$i].bytes -or
      $extractRows[$i].sha256 -cne $result.Rows[$i].sha256) {
    throw "Extracted content mismatch: $($result.Rows[$i].path)"
  }
}

$result.Rows | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding utf8NoBOM
$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash
$summary = @(
  "ZIP=$zipPath",
  "ZIP_SHA256=$zipHash",
  "ZIP_BYTES=$((Get-Item -LiteralPath $zipPath).Length)",
  "ROOTS=$($result.Roots -join ',')",
  "MATCHED_SIZE_AND_SHA256=$($result.Count)/$($result.Count)",
  "MANIFEST=$manifestPath",
  "EXTRACT=$extractRoot"
) -join "`r`n"
[IO.File]::WriteAllText($verifyLogPath, $summary + "`r`n", [Text.UTF8Encoding]::new($false))
$summary
