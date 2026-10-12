# A0–A5 AutoSeq、CAT 与 PTT 故障反馈指南

更新：2026-10-06。本文说明当前本机导出脚本实际收集的数据和反馈时应提供的上下文；不声明新字段或新 schema。A5 的候选版本号尚未指定，版本应从实际运行程序和 `report.txt` 读取。

## 生成与核对反馈 ZIP

1. 在实际运行的 JTDX 安装包中使用配套 `export-jtdx-diagnostics.cmd` 与 `.ps1`，双击 `.cmd`。工具只读本机文件，不启动 JTDX、不连接设备，也不自动上传。默认让用户输入故障发生的本地时间；回车表示现在。默认窗口为事件前 30 分钟、后 10 分钟，按本机时区转换为 UTC，并在 `report.txt` 显示本地时间、时区 ID、事件 UTC 与 UTC 窗口。
2. 若记录的是 UTC，先换算为电脑本地时间输入；反馈时同时写 UTC、当地日期时间和时区。命令行参数、明确时区、输出目录和窗口调整方式见 [P032 导出参数说明](P032_故障反馈导出指南_zh-CN.md)。
3. 默认 ZIP 写到桌面。记录工具显示的完整 ZIP 路径。打开 ZIP，逐项检查 `report.txt`、`settings/AutoSeq-settings.txt` 和每个 `logs/` 条目，再决定是否分享。截图需另作附件；导出器不读取或加入截图，用户自行分享。

导出是一次本地快照，不会等候故障窗口结束。若立即导出时“后 10 分钟”还在未来，ZIP 里不可能有之后才写入的日志；报告仍会列出所选时间窗。需要完整后续窗口时，在窗口结束后重新导出，并以新生成的 ZIP 为准，不要把第一次 ZIP 描述成含有未来记录。

## ZIP 的三层隐私边界

| 数据层 | 当前导出行为 | 分享前注意 |
|---|---|---|
| `JTDX.ini` 白名单摘要 | 仅摘取脚本列出的 AutoSeq 与有限 Rig/PTT 配置键；端点字段不在白名单内。非标准值会隐藏，原始 `@Variant` 不复制。 | 这是配置摘要边界，不代表其余日志已脱敏。摘要反映导出时保存的设置，不证明故障当时的设置。 |
| PTT 请求专用诊断事件 | `ptt_request_*` 与 `ptt_audio_start_refused` 事件不写入原始 TX payload 或 payload identity；可能包含 `call`、`requested_logical_target` / `current_logical_target`、频段/模式和请求关联状态，且可记录 `tx_payload_changed` 布尔值。 | 呼号/目标本身可能识别通联对象。不要将这一专用事件的边界扩大成“整份 ZIP 不含 TX 文本”。 |
| 时间窗内的原始日志行 | 恢复日志和 `ALL.TXT` 中可解析且落在窗口内的原行会被保留；导出器不做全包内容脱敏。 | 原行可能含呼号、QSO/消息/TX 文本或其他现场信息。端点是否实际出现在某条日志里尚未核实，不能承诺 ZIP 不含端点；请检查所有条目并按需要自行遮盖副本后再分享。 |

导出器会限制读取长度和 ZIP 条目大小，但这些上限不是隐私过滤。恢复日志写入时会处理换行符并限制行长；这也不是对呼号、地址或消息内容的脱敏。若不愿分享某条原始行，请不要上传原 ZIP；可先另存并人工遮盖副本，保留原始证据在本机。

## 反馈表

```text
软件显示版本（界面）/ report.txt FileVersion、ProductVersion：
实际运行的候选 ZIP ID 或 SHA-256（未知则写“未记录”）：
故障时间 UTC：
故障时间（本地日期时间）：                 时区：
反馈 ZIP 完整路径：
report.txt 中的事件 UTC、窗口和文件版本是否与本次故障对应：
当前模式（例如 FTX-1 + CAT；只写实际使用的模式）：
应用状态/界面提示/错误文字：
设备侧实际观察（PTT 指示、输出/电源、音频或电台显示；逐项说明实际观察）：
相关截图（单独附件）：
可关联的现有日志字段及值（缺失的字段写“未记录/未知”）：
其他补充：
```

不要默认填写或请求导出串口、网络、TCI、USB/PTT 端点。若 `jtdx_recovery.log` 中有相关行，可抄录实际出现的字段和值，并说明来源；不要推测软件中未记录的端口、句柄或硬件状态。注意不同事件未必同时包含所有关联字段：

- 请求 ID：`ptt_request_serial`；请求代次：`ptt_request_generation`；同期 rig/state 代次：`rig_generation`、`state_generation`。
- 与 CAT 命令有关时，按原行填写 `sequence` 和/或 `command_sequence`；不要把它们当成请求 ID。
- PTT 读回关联可填写 `ptt_request_readback_baseline`、`ptt_readback_serial`、`ptt_readback_utc`、`ptt_readback_age_ms`。
- 来源填写原字段 `ptt_source`，例如日志实际写出的 `rig_get_ptt`、`legacy_state`、`readback_unknown` 或 `none`。它描述软件观察来源，不证明射频输出或外部电台状态。
- 事件时间可对照 `utc` / `local`；只报告 ZIP 原行中实际存在的值。字段缺失写“未记录/未知”，不要从附近行自行补齐。

报告中的版本/构建元数据、时间窗、来源清单和各计数来自当前导出报告；报告可能注明某个 `jtdx.exe` 未找到或字段不可用。请确认它对应实际安装目录，另填用户实际看到的应用版本及所用候选 ZIP ID/hash。ZIP 不提供额外的 schema-version 字段，本阶段也没有增加此类字段。

## 证据边界

JTDX 界面里的 PTT intent、CAT software readback、API 成功或日志 source 标签都不能单独证明电台已经发射/停止发射、RF 输出、电源状态或真实 COM 句柄已释放。把应用内观察与设备侧实际观察分开填写；`unknown` 就保留为未知。此表帮助复现软件状态，不替代 CAT/PTT/TX、HIL 或设备验收。
