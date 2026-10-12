[CmdletBinding()]
param(
    [string]$DataDirectory,
    [string]$IncidentTime,
    [string]$TimeZoneId,
    [ValidateRange(0, 1440)][int]$BeforeMinutes = 30,
    [ValidateRange(0, 1440)][int]$AfterMinutes = 10,
    [string]$OutputDirectory,
    [string]$OutputPath,
    [string]$ExecutablePath
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
$script:Culture = [Globalization.CultureInfo]::InvariantCulture
$script:MaxEntryBytes = 2MB
$script:MaxLineChars = 65536
$script:MaxIniBytes = 1MB
$script:IniKeys = @(
    'AutoSequence','CallMode','CallPriorityCQ','TxFirst','RRR/RR73',
    'SeqAnswerCQCount','SeqAnswerCQCounterValue',
    'SeqAnswerInCallCount','SeqAnswerInCallCounterValue',
    'SeqSentRReportCount','SeqSentRReportCounterValue',
    'SeqSentRR7373Count','SeqSentRR7373CounterValue',
    'SeqHaltTxReplyOther','StrictDirectionalCQ','SingleShotQSO',
    'AutoQSOLogging','ClearCallGrid','TxWatchdogTimer','73RR73Marker',
    'newDXCC','newDXCCBand','newDXCCBandMode',
    'newCall','newCallBand','newCallBandMode',
    'newGrid','newGridBand','newGridBandMode',
    'SeqAutoCallNewDXCC','SeqAutoCallNewDXCCBandMode',
    'SeqAutoCallNewGrid','SeqAutoCallNewGridBandMode',
    'SeqAutoCallNewCall','SeqAutoCallNewCallBand',
    'Rig','PTTMethod','Polling','TxDelayInt','CATSerialRate',
    'CATDataBits','CATStopBits','CATHandshake','RigShare_ptt','SplitMode'
)

# Read only the length captured when the file is opened. The line buffer is capped
# so a damaged or unusually long record cannot consume unbounded memory.
function Initialize-ReaderType {
    if ('P032SnapshotReader' -as [type]) { return }
    $source = @"
using System;
using System.IO;
using System.Text;

public sealed class P032SnapshotReader : IDisposable
{
    private sealed class LimitedStream : Stream
    {
        private readonly Stream inner;
        private readonly long limit;
        private long read;
        public LimitedStream(Stream value, long length) { inner = value; limit = length; }
        public override bool CanRead { get { return inner.CanRead; } }
        public override bool CanSeek { get { return false; } }
        public override bool CanWrite { get { return false; } }
        public override long Length { get { return limit; } }
        public override long Position { get { return read; } set { throw new NotSupportedException(); } }
        public override int Read(byte[] b, int o, int c)
        {
            if (read >= limit) return 0;
            c = (int)Math.Min(c, limit - read);
            int n = inner.Read(b, o, c);
            read += n;
            return n;
        }
        public override void Flush() { throw new NotSupportedException(); }
        public override long Seek(long o, SeekOrigin so) { throw new NotSupportedException(); }
        public override void SetLength(long v) { throw new NotSupportedException(); }
        public override void Write(byte[] b, int o, int c) { throw new NotSupportedException(); }
        protected override void Dispose(bool d) { if (d) inner.Dispose(); base.Dispose(d); }
    }

    private readonly StreamReader reader;
    private readonly char[] buffer = new char[8192];
    private readonly int maxChars;
    private int index;
    private int count;
    private int pending = -2;
    public long OriginalLength { get; private set; }
    public long SnapshotLength { get; private set; }
    public bool LastLineOverlong { get; private set; }

    public P032SnapshotReader(string path, int lineLimit, long byteLimit)
    {
        maxChars = lineLimit;
        FileStream file = new FileStream(path, FileMode.Open, FileAccess.Read,
            FileShare.ReadWrite | FileShare.Delete);
        OriginalLength = file.Length;
        SnapshotLength = Math.Min(OriginalLength, byteLimit);
        reader = new StreamReader(new LimitedStream(file, SnapshotLength),
            new UTF8Encoding(false, false), true, 8192);
    }
    private int Next()
    {
        if (pending != -2) { int v = pending; pending = -2; return v; }
        if (index >= count)
        {
            count = reader.Read(buffer, 0, buffer.Length);
            index = 0;
            if (count == 0) return -1;
        }
        return buffer[index++];
    }
    public string ReadLine()
    {
        LastLineOverlong = false;
        StringBuilder line = new StringBuilder();
        bool any = false;
        while (true)
        {
            int v = Next();
            if (v < 0) { if (!any) return null; break; }
            any = true;
            if (v == '\n') break;
            if (v == '\r')
            {
                int n = Next();
                if (n >= 0 && n != '\n') pending = n;
                break;
            }
            if (line.Length < maxChars) line.Append((char)v);
            else LastLineOverlong = true;
        }
        return line.ToString();
    }
    public void Dispose() { reader.Dispose(); }
}
"@
    Add-Type -TypeDefinition $source -Language CSharp
}

function Convert-IsoTime {
    param([string]$Text, [TimeZoneInfo]$Zone)
    $m = [regex]::Match($Text, '^(?<wall>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,7})?)(?<zone>Z|[+-]\d{2}:\d{2})?$')
    if (-not $m.Success) { return $null }
    $wallText = $m.Groups['wall'].Value
    $format = "yyyy-MM-dd'T'HH:mm:ss"
    if ($wallText.Contains('.')) {
        $format += '.' + ('f' * ($wallText.Length - $wallText.LastIndexOf('.') - 1))
    }
    $wall = [datetime]::MinValue
    if (-not [datetime]::TryParseExact($wallText, $format, $script:Culture, [Globalization.DateTimeStyles]::None, [ref]$wall)) {
        return $null
    }
    $wall = [datetime]::SpecifyKind($wall, [DateTimeKind]::Unspecified)
    $suffix = $m.Groups['zone'].Value
    if ($suffix -eq 'Z') { return [DateTimeOffset]::new([datetime]::SpecifyKind($wall, [DateTimeKind]::Utc)) }
    if ($suffix -match '^[+-]') {
        $sign = if ($suffix[0] -eq '-') { -1 } else { 1 }
        $hours = [int]$suffix.Substring(1, 2)
        $minutes = [int]$suffix.Substring(4, 2)
        if ($hours -gt 14 -or $minutes -gt 59 -or ($hours -eq 14 -and $minutes -ne 0)) { return $null }
        try { return [DateTimeOffset]::new($wall, [TimeSpan]::FromMinutes($sign * (($hours * 60) + $minutes))) }
        catch { return $null }
    }
    if ($Zone.IsInvalidTime($wall) -or $Zone.IsAmbiguousTime($wall)) { return $null }
    $utc = [TimeZoneInfo]::ConvertTimeToUtc($wall, $Zone)
    return [DateTimeOffset]::new([datetime]::SpecifyKind($utc, [DateTimeKind]::Utc))
}

function Get-LineUtc {
    param([string]$Line, [TimeZoneInfo]$Zone)
    if ([string]::IsNullOrWhiteSpace($Line)) { return $null }

    # JTDX recovery-log prefixes are local ISO times. Prefer that timestamp;
    # preserve any utc= and local= fields in the raw line as supporting evidence.
    $prefix = [regex]::Match($Line, '^\s*(?<time>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,7})?(?:Z|[+-]\d{2}:\d{2})?)')
    if ($prefix.Success) {
        $value = Convert-IsoTime $prefix.Groups['time'].Value $Zone
        if ($value) { return $value.ToUniversalTime() }
        return $null
    }

    # ALL.TXT uses a UTC timestamp; the parenthesized value is JTDX's timing
    # correction in seconds, not a timezone offset.
    $compact = [regex]::Match($Line, '^\s*(?<time>\d{8}_\d{6}(?:\.\d{1,7})?)(?:\((?<offset>[^)]{1,32})\))?')
    if ($compact.Success) {
        $text = $compact.Groups['time'].Value
        $format = 'yyyyMMdd_HHmmss'
        if ($text.Contains('.')) { $format += '.' + ('f' * ($text.Length - $text.LastIndexOf('.') - 1)) }
        $wall = [datetime]::MinValue
        if (-not [datetime]::TryParseExact($text, $format, $script:Culture, [Globalization.DateTimeStyles]::None, [ref]$wall)) { return $null }
        return [DateTimeOffset]::new([datetime]::SpecifyKind($wall, [DateTimeKind]::Utc))
    }

    $utcField = [regex]::Match($Line, '(?:^|\s)utc=(?<time>\S+)')
    if ($utcField.Success) {
        $value = Convert-IsoTime $utcField.Groups['time'].Value $Zone
        if ($value) { return $value.ToUniversalTime() }
        return $null
    }
    $localField = [regex]::Match($Line, '(?:^|\s)local=(?<time>\S+)')
    if ($localField.Success) {
        $value = Convert-IsoTime $localField.Groups['time'].Value $Zone
        if ($value) { return $value.ToUniversalTime() }
    }
    return $null
}

function Get-MonthSources {
    param([DateTimeOffset]$Start, [DateTimeOffset]$End, [string]$Root)
    $month = [datetime]::new($Start.UtcDateTime.Year, $Start.UtcDateTime.Month, 1, 0, 0, 0, [DateTimeKind]::Utc)
    $last = [datetime]::new($End.UtcDateTime.Year, $End.UtcDateTime.Month, 1, 0, 0, 0, [DateTimeKind]::Utc)
    while ($month -le $last) {
        $name = $month.ToString('yyyyMM', $script:Culture) + '_ALL.TXT'
        [pscustomobject]@{ Name = $name; Path = [IO.Path]::Combine($Root, $name); Entry = 'logs/' + $name; Kind = 'ALL.TXT' }
        $month = $month.AddMonths(1)
    }
}

function Assert-ZipName {
    param([string]$Name)
    if ($Name -notmatch '^(report\.txt|settings/AutoSeq-settings\.txt|logs/jtdx_recovery\.log(?:\.[1-3])?|logs/\d{6}_ALL\.TXT)$' -or $Name.Contains('\') -or $Name.Contains(':') -or ($Name.Split('/') -contains '..')) {
        throw 'ZIP 条目名不在安全白名单中。'
    }
    return $Name
}

function Add-FilteredSource {
    param(
        [IO.Compression.ZipArchive]$Zip,
        [string]$Path,
        [string]$EntryName,
        [string]$Kind,
        [DateTimeOffset]$StartUtc,
        [DateTimeOffset]$EndUtc,
        [TimeZoneInfo]$Zone
    )
    $r = [ordered]@{ Name=[IO.Path]::GetFileName($Path); Kind=$Kind; Status='缺失'; InputBytes=0L; InputTruncated=$false; Scanned=0L; InWindow=0L; Written=0L; Outside=0L; BadTime=0L; Overlong=0L; Omitted=0L; OutputBytes=0L; Changed=$false; Entry=''; Note='来源文件不存在。' }
    if (-not [IO.File]::Exists($Path)) { return [pscustomobject]$r }

    $reader = $null; $entry = $null; $writer = $null; $stream = $null
    $startWrite = [IO.File]::GetLastWriteTimeUtc($Path)
    $startLength = 0L
    try {
        $maxRead = if ($Kind -eq 'settings') { $script:MaxIniBytes } else { [long]::MaxValue }
        $reader = New-Object P032SnapshotReader($Path, $script:MaxLineChars, $maxRead)
        $startLength = $reader.OriginalLength
        $r.InputBytes = $reader.SnapshotLength
        $r.InputTruncated = ($reader.SnapshotLength -lt $reader.OriginalLength)
        $r.Status = '已扫描'
        if ($r.InputTruncated) { $r.Note = '配置读取达到 1 MiB 上限，尾部未检查。' } else { $r.Note = '' }

        if ($Kind -eq 'settings') {
            $values = @{}
            while ($true) {
                $line = $reader.ReadLine(); $longLine = $reader.LastLineOverlong
                if ($null -eq $line) { break }
                $r.Scanned++
                if ($longLine) { $r.Overlong++; continue }
                $s = $line.Trim()
                if (-not $s -or $s.StartsWith(';') -or $s.StartsWith('#')) { continue }
                $eq = $s.IndexOf('=')
                if ($eq -lt 1) { continue }
                $key = $s.Substring(0, $eq).Trim()
                if ($script:IniKeys -notcontains $key) { continue }
                $value = $s.Substring($eq + 1).Trim()
                if ($key -eq 'Rig') {
                    if ($value.Length -gt 96 -or $value -notmatch '^[A-Za-z0-9][A-Za-z0-9 _().+-]*$') { $value = '[非标准值已隐藏]' }
                } elseif ($key -in @('CATDataBits','CATStopBits','CATHandshake','SplitMode')) {
                    if ($value.StartsWith('@Variant(', [StringComparison]::Ordinal)) {
                        $value = '[已保存但无法安全解码]'
                    } else {
                        $enumMaps = @{
                            CATDataBits = @{ '7'='seven_data_bits'; '8'='eight_data_bits'; '9'='default_data_bits'; 'seven_data_bits'='seven_data_bits'; 'eight_data_bits'='eight_data_bits'; 'default_data_bits'='default_data_bits' }
                            CATStopBits = @{ '1'='one_stop_bit'; '2'='two_stop_bits'; '3'='default_stop_bits'; 'one_stop_bit'='one_stop_bit'; 'two_stop_bits'='two_stop_bits'; 'default_stop_bits'='default_stop_bits' }
                            CATHandshake = @{ '0'='handshake_default'; '1'='handshake_none'; '2'='handshake_XonXoff'; '3'='handshake_hardware'; 'handshake_default'='handshake_default'; 'handshake_none'='handshake_none'; 'handshake_XonXoff'='handshake_XonXoff'; 'handshake_hardware'='handshake_hardware' }
                            SplitMode = @{ '0'='split_mode_none'; '1'='split_mode_rig'; '2'='split_mode_emulate'; 'split_mode_none'='split_mode_none'; 'split_mode_rig'='split_mode_rig'; 'split_mode_emulate'='split_mode_emulate' }
                        }
                        $map = $enumMaps[$key]
                        if ($map.ContainsKey($value)) {
                            if ($value -match '^\d{1,2}$') { $value = $value + ' (' + $map[$value] + ')' }
                            else { $value = $map[$value] }
                        } else {
                            $value = '[非标准值已隐藏]'
                        }
                    }
                } elseif ($key -eq 'CATSerialRate') {
                    if ($value -notin @('1200','2400','4800','9600','19200','38400','57600','115200')) { $value = '[非标准值已隐藏]' }
                } elseif ($key -in @('Polling','TxDelayInt')) {
                    if ($value -notmatch '^\d{1,3}$') { $value = '[非标准值已隐藏]' }
                    else {
                        $numericValue = [int]$value
                        $maximum = if ($key -eq 'Polling') { 999 } else { 500 }
                        if ($numericValue -gt $maximum) { $value = '[非标准值已隐藏]' }
                    }
                } elseif ($key -eq 'RigShare_ptt') {
                    if ($value -notmatch '^(?i:true|false|0|1)$') { $value = '[非标准值已隐藏]' }
                } elseif ($key -in @('CallMode','TxWatchdogTimer','PTTMethod','SeqAnswerCQCounterValue','SeqAnswerInCallCounterValue','SeqSentRReportCounterValue','SeqSentRR7373CounterValue')) {
                    if ($value -notmatch '^-?\d{1,3}$') { $value = '[非标准值已隐藏]' }
                    elseif ($key -eq 'PTTMethod') {
                        $pttNames = @{ '0'='VOX'; '1'='CAT'; '2'='DTR'; '3'='RTS' }
                        if ($pttNames.ContainsKey($value)) { $value = $value + ' (' + $pttNames[$value] + ')' }
                        else { $value = '[非标准值已隐藏]' }
                    }
                } elseif ($value -notmatch '^(?i:true|false|0|1)$') { $value = '[非标准值已隐藏]' }
                $values[$key] = $value
            }
            $safe = @('来源：JTDX.ini AutoSeq 与安全 rig 会话配置白名单摘要','说明：没有复制完整 INI 文件；@Variant 值只显示固定的不可安全解码标记。')
            foreach ($key in $script:IniKeys) {
                $safe += if ($values.ContainsKey($key)) { $key + '=' + $values[$key] } else { $key + '=[未保存]' }
            }
            $entry = $Zip.CreateEntry((Assert-ZipName $EntryName))
            $stream = $entry.Open()
            $writer = New-Object IO.StreamWriter($stream, (New-Object Text.UTF8Encoding($false)), 4096)
            foreach ($line in $safe) { $writer.WriteLine($line) }
            $writer.Flush()
            $r.Written = $safe.Count
            $r.OutputBytes = $writer.BaseStream.Length
            $r.Entry = $EntryName
            $r.Status = '白名单摘要'
            $writer.Dispose(); $writer = $null; $stream.Dispose(); $stream = $null
        } else {
            $entry = $Zip.CreateEntry((Assert-ZipName $EntryName))
            $stream = $entry.Open()
            $writer = New-Object IO.StreamWriter($stream, (New-Object Text.UTF8Encoding($false)), 4096)
            $capped = $false
            while ($true) {
                $line = $reader.ReadLine(); $longLine = $reader.LastLineOverlong
                if ($null -eq $line) { break }
                $r.Scanned++
                if ($longLine) { $r.Overlong++; continue }
                $time = Get-LineUtc $line $Zone
                if ($null -eq $time) { if (-not [string]::IsNullOrWhiteSpace($line)) { $r.BadTime++ }; continue }
                if ($time -lt $StartUtc -or $time -gt $EndUtc) { $r.Outside++; continue }
                $r.InWindow++
                $bytes = [Text.Encoding]::UTF8.GetByteCount($line) + [Text.Encoding]::UTF8.GetByteCount([Environment]::NewLine)
                if ($capped -or ($r.OutputBytes + $bytes) -gt $script:MaxEntryBytes) { $capped = $true; $r.Omitted++; continue }
                $writer.WriteLine($line)
                $r.OutputBytes += $bytes
                $r.Written++
            }
            $writer.Flush(); $writer.Dispose(); $writer = $null; $stream.Dispose(); $stream = $null
            if ($r.Written -gt 0) { $r.Entry = $EntryName; $r.Status = '已筛选'; $r.Note = '' }
            else {
                $entry.Delete(); $entry = $null
                $r.Status = '窗口内无可导出记录'
                $r.Note = '无有效时间戳记录落在 UTC 窗口内。'
            }
        }
        $reader.Dispose(); $reader = $null
        $after = New-Object IO.FileInfo($Path); $after.Refresh()
        $r.Changed = ($after.Length -ne $startLength) -or ($after.LastWriteTimeUtc -ne $startWrite)
        if ($r.Changed) { $r.Note += ' 读取期间文件变化；只读打开时捕获的长度。' }
        if ($r.Overlong -gt 0) { $r.Note += ' 超过 65536 字符的行已排除。' }
        if ($r.BadTime -gt 0) { $r.Note += ' 无法解析时间戳的行已排除。' }
        if ($r.Omitted -gt 0) { $r.Note += ' 每文件 2 MiB 输出上限已省略后续窗口内行。' }
        if ($r.InputTruncated) { $r.Note += ' 配置尾部超过读取上限。' }
        return [pscustomobject]$r
    } catch {
        if ($writer) { try { $writer.Dispose() } catch {} }
        if ($stream) { try { $stream.Dispose() } catch {} }
        if ($reader) { try { $reader.Dispose() } catch {} }
        if ($entry) { try { $entry.Delete() } catch {} }
        $r.Status = '读取失败'; $r.Note = '来源文件无法读取或处理。'
        return [pscustomobject]$r
    }
}

function Get-PeBuildInfo {
    param([string]$Path)
    $result = [ordered]@{ Status='未找到'; Name='jtdx.exe'; FileVersion='不可用'; ProductVersion='不可用'; PeTimestampUtc='不可用'; ModifiedUtc='不可用' }
    if (-not [IO.File]::Exists($Path)) { return [pscustomobject]$result }
    $result.Status = '已读取文件元数据，未启动程序'
    try {
        $v = [Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
        if ($v.FileVersion) { $result.FileVersion = $v.FileVersion }
        if ($v.ProductVersion) { $result.ProductVersion = $v.ProductVersion }
        $info = New-Object IO.FileInfo($Path)
        $result.ModifiedUtc = $info.LastWriteTimeUtc.ToString('yyyy-MM-dd HH:mm:ss ''UTC''', $script:Culture)
        $f = [IO.File]::OpenRead($Path)
        try {
            if ($f.Length -ge 80) {
                $br = New-Object IO.BinaryReader($f)
                try {
                    $f.Position = 0x3c; $pe = $br.ReadInt32()
                    if ($pe -ge 0 -and ($pe + 12) -le $f.Length) {
                        $f.Position = $pe
                        if ($br.ReadUInt32() -eq 0x00004550) {
                            $f.Position = $pe + 8; $stamp = $br.ReadUInt32()
                            if ($stamp -gt 0) { $result.PeTimestampUtc = [DateTimeOffset]::FromUnixTimeSeconds([long]$stamp).ToString('yyyy-MM-dd HH:mm:ss ''UTC''', $script:Culture) }
                        }
                    }
                } finally { $br.Dispose() }
            }
        } finally { $f.Dispose() }
    } catch { $result.Status = '元数据部分读取失败；程序未启动' }
    return [pscustomobject]$result
}

$interactive = [string]::IsNullOrWhiteSpace($IncidentTime)
if (-not $DataDirectory) {
    if (-not $env:LOCALAPPDATA) { throw 'LOCALAPPDATA 未设置，请使用 -DataDirectory 指定 JTDX 数据目录。' }
    $DataDirectory = [IO.Path]::Combine($env:LOCALAPPDATA, 'JTDX')
}
$DataDirectory = [IO.Path]::GetFullPath($DataDirectory)
$zone = if ($TimeZoneId) { [TimeZoneInfo]::FindSystemTimeZoneById($TimeZoneId) } else { [TimeZoneInfo]::Local }
$TimeZoneId = $zone.Id

if ($interactive) {
    Write-Host 'JTDX 故障反馈导出：只读本地文件，不会上传。'
    Write-Host ('数据目录：' + $DataDirectory + '；本地时区：' + $TimeZoneId)
    $inputTime = Read-Host '故障发生本地时间 yyyy-MM-dd HH:mm[:ss]（回车=现在）'
    $IncidentTime = if ($inputTime) { $inputTime.Trim() } else { [datetime]::Now.ToString('yyyy-MM-dd HH:mm:ss', $script:Culture) }
}
if ($BeforeMinutes -lt 0 -or $BeforeMinutes -gt 1440 -or $AfterMinutes -lt 0 -or $AfterMinutes -gt 1440) {
    throw '时间窗前后分钟须为 0 到 1440 的整数。'
}

$formats = @('yyyy-MM-dd HH:mm:ss','yyyy-MM-dd HH:mm:ss.FFFFFFF',"yyyy-MM-dd'T'HH:mm:ss","yyyy-MM-dd'T'HH:mm:ss.FFFFFFF",'yyyy-MM-dd HH:mm')
$wall = [datetime]::MinValue
if (-not [datetime]::TryParseExact($IncidentTime, [string[]]$formats, $script:Culture, [Globalization.DateTimeStyles]::None, [ref]$wall)) {
    throw '事件时间格式无效，请使用 yyyy-MM-dd HH:mm[:ss[.fff]]。'
}
$wall = [datetime]::SpecifyKind($wall, [DateTimeKind]::Unspecified)
if ($zone.IsInvalidTime($wall) -or $zone.IsAmbiguousTime($wall)) { throw '该本地时间在所选时区无效或存在歧义，请改用明确时间。' }
$incidentLocal = [DateTimeOffset]::new($wall, $zone.GetUtcOffset($wall))
$incidentUtc = $incidentLocal.ToUniversalTime()
$startUtc = $incidentUtc.AddMinutes(-$BeforeMinutes)
$endUtc = $incidentUtc.AddMinutes($AfterMinutes)

if ($OutputPath) {
    $OutputPath = [IO.Path]::GetFullPath($OutputPath)
    if ([IO.Path]::GetExtension($OutputPath) -ine '.zip') { throw '输出文件扩展名必须是 .zip。' }
    if ([IO.File]::Exists($OutputPath)) { throw '指定 ZIP 已存在；请指定新文件名，导出器不会覆盖。' }
    if (-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($OutputPath))) { throw 'ZIP 输出目录不存在。' }
} else {
    if (-not $OutputDirectory) { $OutputDirectory = [Environment]::GetFolderPath([Environment+SpecialFolder]::DesktopDirectory) }
    if (-not $OutputDirectory) { throw '未能解析桌面目录；请使用 -OutputDirectory 或 -OutputPath 指定 ZIP 输出位置。' }
    if (-not [IO.Directory]::Exists($OutputDirectory)) { throw ('ZIP 输出目录不存在：' + $OutputDirectory + '。请使用 -OutputDirectory 或 -OutputPath 指定可用位置。') }
    $OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
    $stem = 'JTDX故障反馈_' + $incidentLocal.ToString('yyyyMMdd_HHmmss', $script:Culture)
    $OutputPath = [IO.Path]::Combine($OutputDirectory, $stem + '.zip')
    $n = 1
    while ([IO.File]::Exists($OutputPath)) {
        $OutputPath = [IO.Path]::Combine($OutputDirectory, $stem + '_' + $n.ToString('00') + '.zip')
        $n++
        if ($n -gt 999) { throw '无法生成不重名的 ZIP 文件名。' }
    }
}

Initialize-ReaderType
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$sources = @()
for ($i = 0; $i -le 3; $i++) {
    $suffix = if ($i -eq 0) { '' } else { '.' + $i }
    $name = 'jtdx_recovery.log' + $suffix
    $sources += [pscustomobject]@{ Path=[IO.Path]::Combine($DataDirectory,$name); Entry='logs/' + $name; Kind='recovery' }
}
$sources += Get-MonthSources $startUtc $endUtc $DataDirectory
$sources += [pscustomobject]@{ Path=[IO.Path]::Combine($DataDirectory,'JTDX.ini'); Entry='settings/AutoSeq-settings.txt'; Kind='settings' }

$exe = $ExecutablePath
if (-not $exe) {
    $near = @([IO.Path]::Combine($PSScriptRoot,'jtdx.exe'),[IO.Path]::Combine([IO.Path]::GetDirectoryName($PSScriptRoot),'jtdx.exe'))
    if ($env:ProgramFiles) { $near += [IO.Path]::Combine($env:ProgramFiles,'JTDX','bin','jtdx.exe') }
    $pf86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    if ($pf86) { $near += [IO.Path]::Combine($pf86,'JTDX','bin','jtdx.exe') }
    $exe = $near | Where-Object { [IO.File]::Exists($_) } | Select-Object -First 1
}
$exeInfo = if ($exe) { Get-PeBuildInfo ([IO.Path]::GetFullPath($exe)) } else { [pscustomobject]@{ Status='未找到 jtdx.exe'; Name='jtdx.exe'; FileVersion='不可用'; ProductVersion='不可用'; PeTimestampUtc='不可用'; ModifiedUtc='不可用' } }

$zip = $null; $outputStream = $null; $created = $false; $records = @()
try {
    try {
        $outputStream = [IO.File]::Open($OutputPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    } catch {
        throw ('无法在 ZIP 输出位置创建新文件：' + $OutputPath + '。请检查写入权限，或使用 -OutputDirectory / -OutputPath 指定可写位置。原错误：' + $_.Exception.Message)
    }
    $created = $true
    $zip = New-Object IO.Compression.ZipArchive($outputStream,[IO.Compression.ZipArchiveMode]::Create,$false)
    $outputStream = $null
    foreach ($source in $sources) {
        $records += Add-FilteredSource -Zip $zip -Path $source.Path -EntryName $source.Entry -Kind $source.Kind -StartUtc $startUtc -EndUtc $endUtc -Zone $zone
    }
    $missing = @($records | Where-Object { $_.Status -eq '缺失' })
    $failed = @($records | Where-Object { $_.Status -eq '读取失败' })
    $changed = @($records | Where-Object { $_.Changed })
    $limited = @($records | Where-Object { $_.InputTruncated -or $_.Omitted -gt 0 -or $_.Overlong -gt 0 })
    $lines = New-Object 'System.Collections.Generic.List[string]'
    [void]$lines.Add('JTDX 故障反馈导出报告')
    [void]$lines.Add(('事件本地时间：' + $incidentLocal.ToString('yyyy-MM-dd HH:mm:ss.fff zzz',$script:Culture)))
    [void]$lines.Add(('时区 ID：' + $TimeZoneId))
    [void]$lines.Add(('事件 UTC：' + $incidentUtc.ToString('yyyy-MM-dd HH:mm:ss.fff ''UTC''',$script:Culture)))
    [void]$lines.Add(('UTC 时间窗（含端点）：' + $startUtc.ToString('yyyy-MM-dd HH:mm:ss.fff ''UTC''',$script:Culture) + ' 至 ' + $endUtc.ToString('yyyy-MM-dd HH:mm:ss.fff ''UTC''',$script:Culture)))
    [void]$lines.Add(('窗口：前 ' + $BeforeMinutes + ' 分钟，后 ' + $AfterMinutes + ' 分钟。'))
    [void]$lines.Add(('版本与构建：状态=' + $exeInfo.Status + '；文件=' + $exeInfo.Name + '；FileVersion=' + $exeInfo.FileVersion + '；ProductVersion=' + $exeInfo.ProductVersion + '；PE 时间戳=' + $exeInfo.PeTimestampUtc + '；文件修改 UTC=' + $exeInfo.ModifiedUtc))
    [void]$lines.Add('应用程序仅按 PE 元数据读取，未启动。')
    [void]$lines.Add('时间解析：Recovery 日志行首 ISO 时间按所选本地时区解析；带 Z/偏移的时间按其偏移解析；新日志行中的 utc= / local= 字段原样保留作辅助。ALL.TXT 的 yyyyMMdd_HHmmss 时间戳按 UTC 解析，括号内是 JTDX 校时秒数。')
    [void]$lines.Add('安全边界：只导出时间窗内可解析日志行和明确白名单设置摘要；不复制完整 INI、完整 ALL.TXT 或其他系统文件；没有上传。')
    [void]$lines.Add('单个日志条目输出上限：2 MiB；配置文件最多读取 1 MiB；单行最多处理 65536 字符。')
    [void]$lines.Add('')
    [void]$lines.Add('来源清单（输入字节数为打开时快照长度）：')
    foreach ($r in $records) {
        [void]$lines.Add('- ' + $r.Name + ' | ' + $r.Kind + ' | ' + $r.Status +
            ' | 输入=' + $r.InputBytes + ' | 扫描行=' + $r.Scanned +
            ' | 窗口内=' + $r.InWindow + ' | 写入=' + $r.Written +
            ' | 窗口外=' + $r.Outside + ' | 无效时间=' + $r.BadTime +
            ' | 超长行=' + $r.Overlong + ' | 上限省略=' + $r.Omitted +
            ' | 读取时变化=' + $(if ($r.Changed) { '是' } else { '否' }) +
            ' | ZIP 条目=' + $(if ($r.Entry) { $r.Entry } else { '无' }) +
            $(if ($r.Note) { ' | ' + $r.Note } else { '' }))
    }
    [void]$lines.Add('')
    [void]$lines.Add('缺失来源：' + $(if ($missing.Count) { ($missing | ForEach-Object { $_.Name }) -join '、' } else { '无' }))
    [void]$lines.Add('读取失败来源：' + $(if ($failed.Count) { ($failed | ForEach-Object { $_.Name }) -join '、' } else { '无' }))
    [void]$lines.Add('读取期间变化来源：' + $(if ($changed.Count) { ($changed | ForEach-Object { $_.Name }) -join '、' } else { '无' }))
    [void]$lines.Add('受限或省略来源：' + $(if ($limited.Count) { ($limited | ForEach-Object { $_.Name }) -join '、' } else { '无' }))
    [void]$lines.Add('部分成功说明：缺失、无法解析时间戳、超长行或达到输出上限的来源均在本报告列出；没有可用日志时仍保留报告和清单。')
    [void]$lines.Add('JTDX.ini 只摘录 AutoSeq 白名单以及 Polling、TxDelayInt、CATSerialRate、DataBits、StopBits、Handshake、RigShare_ptt、SplitMode 等安全 rig 配置键；枚举只接受固定数值/名称，@Variant 显示为“已保存但无法安全解码”，其他非标准值会隐藏。串口、网络、TCI、USB/PTT 端点及其他 INI 字段不会导出。当前配置不能证明故障当时设置。')
    [void]$lines.Add('请先检查本报告，再由用户自行决定是否分享 ZIP。本工具不会自动发送文件。')
    $entry = $zip.CreateEntry((Assert-ZipName 'report.txt'))
    $stream = $entry.Open()
    try {
        $writer = New-Object IO.StreamWriter($stream,(New-Object Text.UTF8Encoding($false)),4096)
        foreach ($line in $lines) { $writer.WriteLine([string]$line) }
        $writer.Flush()
    } finally { if ($writer) { $writer.Dispose() } else { $stream.Dispose() } }
    $zip.Dispose(); $zip = $null
} catch {
    if ($zip) { try { $zip.Dispose() } catch {} }
    if ($outputStream) { try { $outputStream.Dispose() } catch {} }
    if ($created -and [IO.File]::Exists($OutputPath)) { try { [IO.File]::Delete($OutputPath) } catch {} }
    throw
}

Write-Host ('已生成 ZIP：' + $OutputPath)
Write-Host ('来源 ' + $records.Count + ' 项；缺失 ' + $missing.Count + ' 项；读取失败 ' + $failed.Count + ' 项。')
Write-Host '请将 ZIP 和相关截图一起反馈给支持人员；工具不会自动上传。'
return $OutputPath
