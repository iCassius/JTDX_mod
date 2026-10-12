# P035 CAT 与 PTT 状态反馈指南

本指南对应 2026-10-05 生成的 P035 本机审阅 ZIP；该包不含 A1–A4 工作树改动，也不是当前 A0–A5 候选。P034 曾出现严重现场回归，已撤回长挂机验收建议；其原始 ZIP、说明和源码快照保留用于追查，不应作为长挂机验证包。P035 修正 queued PTT SET 的 Qt 参数注册与 Hamlib 成功 GET 的 health 结果上报，但软件回归还不能证明现场 15 秒收发周期恢复或 long-idle COM 释放根因。

## CAT/PTT 观察边界

CAT health observer 观察当前会话中支持的 poll/GET 结果，不自动重连、关闭串口或改变 TX。5 秒阈值只用于把未完成的长 poll 标记为观察性 `suspect`，不是 PTT readiness 超时。FTX-1 + CAT 的成功 GET 经 protocol generation guard 和 readback 确认后才报告 health 成功；Hamlib `RIG_OK` 不证明串口线路或电台已完成往返。其他后端可能使用 legacy 状态来源，不能把软件状态边沿当作真实硬件 GET。

点击 `readFreq` 时，程序先保留旧 `sync_transceiver(true)` 同步调用，再另行请求 PTT GET。独立 GET 不会重连 rig 或写 CAT；旧同步动作使用默认 `enforce_mode_and_split=false`，仍可能有 split-off/TX 频率清零兼容副作用。因此不能把整个按钮称为完全无副作用的纯 GET。

## 生成反馈 ZIP

在实际安装目录确认应用版本为 P035，然后双击该目录内的 `bin\export-jtdx-diagnostics.cmd`。按提示输入故障发生的本地时间（`yyyy-MM-dd HH:mm[:ss]`；回车表示现在）；默认收集此前 30 分钟至此后 10 分钟，ZIP 保存到桌面，工具会打印完整路径。打开 ZIP 中的 `report.txt` 检查摘要。若时间记录为 UTC，先换算电脑本地时间输入，并在反馈中同时注明原始 UTC、时区和本地时间；命令行时间窗和输出路径参数见 [P032 故障反馈导出指南](P032_故障反馈导出指南_zh-CN.md)。工具只在本机读取限定日志和配置白名单，不自动上传。

反馈时请提供实际运行版本、故障 UTC/本地时间及时区、相关界面截图和刚生成的 ZIP；截图需另作附件发送。JTDX.ini 白名单摘要不含端点字段、凭据或原始 `@Variant` 值；`ptt_request_*` 与 `ptt_audio_start_refused` 专用事件不写原始 TX payload 或 payload identity，但可能含呼号/目标上下文。恢复日志和 ALL.TXT 窗口内原行会保留，可能含消息或 TX 文本；端点是否实际出现在这些原行中尚未核实，因此不能保证整份 ZIP 不含端点或 TX 文本。分享前检查 report 和全部 ZIP 条目，字段说明见 [A0–A5 故障反馈指南](A0-A5_故障反馈指南_zh-CN.md)。历史 `004430` 与 `052800` 缺少充分的 request/generation/readback/context 关联字段，不能据此判断同根因；请把反馈中的软件观察与实际设备证据分开描述。

## P0-TX S3 停止诊断反馈

针对本次停止/关闭链诊断，反馈只需提供故障发生的 UTC 时间、实际运行的 app version 和日志 bundle。先完全退出应用，再从本机复制 `jtdx_recovery.log` 及当时存在的轮转文件到本地 bundle；不要收集或附带 `JTDX.ini`。新加的停止日志只描述软件请求、driver 方法结果和软件 API 读回；界面状态与日志都不能证明 PTT 线路或 RF 已关闭。

## 验证限制

P035 生产 alias 注册的 Qt MOC/QThread fake 回归和 Hamlib 成功 GET 接线验证不是完整 JTDX GUI、真实 CAT/PTT/TX、COM long-idle、HIL 或 `159` 验收。尚未证明现场 15 秒恢复或 COM 释放根因；P035 也不是公开发行。任何将来的实机验收都应先验证短时正常收发周期和 PTT 状态，再进入 long-idle 观察；软件测试不能替代这一步，也不能支持直接挂机的结论。

## 2026-10-05 A1 readFreq后续校正

上文关于 `readFreq` 保留单参数 `sync_transceiver(true)` 并延续默认 split-off/TX频率清零副作用的说明有误；原文保留作历史记录。A1已恢复基线旧行为：`transceiver_online()` 为门控，在线时调用 `sync_transceiver(true, true)`，然后请求一次独立只读 CAT refresh；offline 不 sync/GET，不引入自动连接或重连。

针对性测试编译了当前生产 `MainWindow::on_readFreq_clicked` 与 `Configuration::sync_transceiver` 函数体，并用fake adapter捕获清零调用；online、offline、transmitting及单参数负例均覆盖，2/2测试通过。未验证完整GUI、真实CAT/PTT/TX或设备读回；详细 A1 记录属于内部研发资料，不随运行安装包分发。

## 2026-10-06 A4 文案纠正

原文“导出内容不包含端点……或 PTT 请求 TX 文本”把 INI 白名单与 PTT 专用事件的边界错误地写成整 ZIP 保证，现已由上方三层说明更正。A4 未改导出器或日志数据；历史 ZIP 保持原样。
