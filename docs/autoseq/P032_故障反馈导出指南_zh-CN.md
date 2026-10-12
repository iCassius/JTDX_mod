# P032 故障反馈导出指南

本工具从本机 JTDX 数据目录生成一个 ZIP，供用户检查后自行反馈。它只读本地文件，不启动 JTDX、不连接电台，也不会自动上传。截图请另行附上；截图不会被加入 ZIP。

## 普通双击

保持 `export-jtdx-diagnostics.cmd` 和 `export-jtdx-diagnostics.ps1` 在同一目录，双击 `.cmd` 文件。程序只询问一次故障发生的本地时间：

- 输入 `2026-10-03 14:25:00` 这类时间；回车表示现在。
- 默认时间窗为事件前 30 分钟至后 10 分钟，按 Windows 当前时区换算。
- ZIP 直接放到桌面。成功时窗口显示 ZIP 完整路径，并提示将 ZIP 和相关截图一起反馈。

默认数据目录为 `%LOCALAPPDATA%\JTDX`。如果该目录下有文件缺失，工具仍会生成 ZIP，并在 `report.txt` 中列明。

## 高级参数

需要更改时间窗、数据目录或输出位置时，从命令行运行脚本。以下示例把前后窗口改为 60/20 分钟，并使用指定输出目录，避免写入实际桌面：

```powershell
Set-Location "<导出脚本所在目录>"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File ".\export-jtdx-diagnostics.ps1" `
  -DataDirectory "$env:LOCALAPPDATA\JTDX" `
  -IncidentTime "2026-10-03 14:25:00" `
  -BeforeMinutes 60 -AfterMinutes 20 `
  -OutputDirectory ".\JTDX-feedback"
```

`-IncidentTime` 使用本地时间。`-TimeZoneId` 可明确指定时区，例如 `-TimeZoneId "China Standard Time"`。前后分钟须为 0 到 1440。`-OutputDirectory` 指向的目录必须已存在；也可以用 `-OutputPath ".\JTDX-feedback\case-01.zip"` 指定 ZIP 完整路径，目标目录必须已存在且 ZIP 文件尚不存在。若桌面不可用或输出文件无法创建，工具会报错并提示使用 `-OutputDirectory` 或 `-OutputPath`，不会改存到其他位置。

## ZIP 内容与限制

ZIP 包含 `report.txt`、事件时间窗内可解析的 `jtdx_recovery.log` 及 `.1` 到 `.3`、事件 UTC 时间窗涉及月份的 `YYYYMM_ALL.TXT` 筛选行，以及 `JTDX.ini` 中明确列出的 AutoSeq/重试阈值、相关开关、Rig 和 PTT 方法白名单摘要。ALL.TXT 时间戳按 UTC 过滤；恢复日志的无时区 ISO 时间按所选本地时区解释。跨 UTC 午夜或月份边界时会检查两侧月份文件。

每个日志 ZIP 条目最多 2 MiB，单行最多处理 65536 字符；JTDX.ini 最多读取 1 MiB。超长行、无效时间戳和达到上限后省略的行都会在报告中计数。报告同时列出缺失/读取失败文件、打开时捕获的输入长度、读取期间观察到的文件变化，以及程序文件的版本和 PE 时间戳元数据。运行中的日志可能继续变化，导出只限制在打开时捕获的文件长度，不能保证多份日志来自同一瞬间。

工具不复制完整 INI 或完整 ALL.TXT；JTDX.ini 项只生成白名单摘要，且不含端点字段或原始 `@Variant`。但恢复日志和 `ALL.TXT` 时间窗内原行没有全包脱敏，可能含呼号、消息/TX 文本或其他现场信息；端点是否实际出现在日志中尚未核实。INI 摘要反映导出时保存的设置，不能证明故障发生时的设置。分享前请打开 ZIP 检查 `report.txt` 与所有日志条目。截图由用户另行附上。通用字段和反馈表见 [A0–A5 故障反馈指南](A0-A5_故障反馈指南_zh-CN.md)。

## 2026-10-06 A4 隐私边界校正

较早版本把 INI 摘要的字段限制写成了对整份 ZIP 的保证。当前准确边界是：INI 白名单不含端点，PTT 专用事件不写原始 TX payload/identity；恢复日志与 ALL.TXT 仍保留窗口内原行，须逐项检查。完整说明见 [A0–A5 故障反馈指南](A0-A5_故障反馈指南_zh-CN.md)。
