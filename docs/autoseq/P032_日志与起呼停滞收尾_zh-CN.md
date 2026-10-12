# P032 AutoSeq 诊断日志与初始起呼停滞收尾

## 目标与现场边界

本阶段为 AutoSeq 首次 CQ/RFIN 选择、Enable Tx、PTT 回读、音频启动和后续解码增加可关联的本地证据；若一个新选中的 AutoSeq 初始目标长时间没有实际 TX 进展，则只在安全门全部满足时释放该目标，避免它永久占用 DX。

触发背景是 2026-10-01 FW1P 的现场反馈：观察到一次 Tx1、PTT 与音频启动，但后续没有第二次 TX，接收解码仍继续。该现场没有在本轮复现；随后出现的 CAT 故障日志也不能证明就是首次起呼停滞的根因。本轮实现与自动化测试不声称重放或解释了这一台站现场。

## 本地结构化日志

日志写入现有应用数据目录中的 `jtdx_recovery.log`，与 `WriteDecodedDebugALLTXT` 无关。默认单文件上限为 2 MiB，保留 3 个轮转档案；每档都受同一字节上限约束。日志写入、轮转或格式化失败会被吞掉，不改变 CAT、AutoSeq 或 TX 流程。归档路径意外成为目录或其他非文件时，logger 会截断活动日志并继续有界追加。

AutoSeq 事件包含本地与 UTC 时间、应用版本/源码 revision/build、会话 ID、AutoSeq attempt ID、实时 decode batch ID、目标呼号、模式/波段、QSO history 状态与 retry count、TX slot、Enable Tx、transmitting、tune、PTT intent、CAT online、PTT known/on/source/time/age、无进展时长及最后一次真实进展类型。`decode_started_utc` 使用保存在该 decode batch 的实际开始时间。呼号和消息字段做百分号编码以保持单行结构。

主要事件包括初始目标选择、目标/操作员取消、60 秒活动快照、释放阻塞原因、同批有效定向回复、实际 PTT 确认、音频启动进展、`haltTx(reason)` 的原始停止原因、PTT 回读失效与最终释放。`initial_target_selected` 不记录上一轮遗留的 `m_curMsgTx`；发送请求、PTT/audio 确认及 halt 事件才附带当时消息文本。

`Configuration` 另外记录运行 rig 的创建、复用、adopt 和关闭原因，以及失败事件的接收 generation、当前 generation、session purpose、command sequence、online/PTT 状态和过滤原因。观察代码使用只读 `is_transceiver_online()` 与现有 `m_rigState`，不调用会打开 rig 的 `transceiver_online()`；失败过滤与路由规则保持原样。

日志和故障反馈 ZIP 可能包含呼号、频率、时间、QSO 阶段和 TX 消息。分享前应检查导出包中的 `report.txt` 与筛选日志。导出工具操作和限制见[故障反馈导出指南](P032_故障反馈导出指南_zh-CN.md)。导出工具的独立验证记录属于内部研发资料，不随运行安装包分发。

## 初始目标所有权与超时策略

所有权只在 AutoSeq 实际选择新的 `RCQ` 或 `RFIN` 候选时建立；手动 DX、从已有 QSO 恢复的目标、Hound/WSPR 路径不获得此初始起呼 owner。计时使用 `steady_clock` 的单调毫秒值，从新目标选择时开始，只由在线 transceiver PTT-on 回读或 SoundOutput 的启动确认/进度信号刷新。第三方解码、重复 CQ/RFIN、Enable Tx 切换和消息槽选择都不刷新计时。

超时为连续 120 秒无上述实际进展。评估只发生在完整、当前、实时的 `DecodeFinished` batch 结束处；每 60 秒最多输出一次活动快照，重复相同阻塞原因会抑制逐 batch 日志。当前目标/波段/模式或 QSO 初始阶段改变、AutoSeq 停用以及手动/磁盘解码都会结束所有权。

结束前先检查本批的目标定向消息，再核对该 owner 最新 `QsoHistory` 阶段。只有同一个基础呼号在本批向本机发出定向消息，且 history 已进入有效的 `RCALL`、`RREPORT`、`RRREPORT`、`RRR`、`RRR73` 或 `R73` 阶段，才取消初始起呼超时，让正常 QSO 继续。`RREPORT` 是有效的定向 QSO 阶段，不作为首次 CQ 超时候选。`skip Tx1` 仅在 history 为 `SREPORT` 且 `m_QSOProgress == REPORT` 时按初始阶段处理。

## 释放安全门与后续行为

释放前必须同时满足：AutoSeq 仍开、当前 DX/波段/模式与 owner 相符、history 仍在初始阶段、rig 在线、PTT 状态已知且为 off、该 PTT-off 回读发生在本次目标选择之后，并且当前没有 transmitting、Tune、`g_iptt`/TX/PTT 请求、恢复流程或其他待处理发射状态。

每次新的真实 PTT 请求发出之前，旧的 PTT-readback 有效位和时间戳都会清除；请求后的在线 transceiver update 才重新建立回读有效性。这样，先前的 PTT-off 读数不能替一个尚未确认的新请求放行释放。若新的在线回读未到达，策略记录 `ptt_unknown` 并保留 DX。PTT 回读年龄会记录用于诊断，但没有设置短时间过期门槛。

通过全部安全门后，程序先把收到的报告/时间加入既有、有限的自动 CQ 冷却记录，再取消 owner、关闭 Enable Tx、清除 DX，并跳过整个当前 batch，避免 `clearDX()` 所触发的 CQ 选择立即重新占用同一目标。下一次完整实时 batch 才可重新选候选；现有普通 3 次 TX 重试和已经进入的 QSO 终止语义未改。

操作员停止 TX、关闭 Enable Tx/AutoSeq、手动改 DX 或切换波段/模式时，会取消 owner 并记录 suppress-rearm 呼号，防止同一自动流程立即跨目标强制重启。只有操作员显式重新开启对应控制，才解除这项 hold。内部 AutoSeq 临时设置 Enable Tx reason 会在动作后恢复旧值；后续日志不会继承 `automatic_initial_candidate` 等旧原因。

## 自动化证据与限制

修复阶段的策略测试覆盖 120 秒边界、实际进展刷新、同批有效回复优先、skip-Tx1 阶段、安全门 fail-closed、阻塞去重和 owner 更换；源契约测试检查 PTT 请求前失效旧回读、在线回读重新有效、同批回复先于释放，以及 CAT 观察不调用副作用 accessor。对应 Release 构建与 34 项 CTest 结果保存在源码仓库的本机候选交付报告；这些属于编译、策略和源码契约证据，不等于 GUI 实际时序或电台行为证明。

本轮没有启动 JTDX、没有连接真实 CAT、没有操作 PTT/TX、没有发射、没有做 HIL 或网络部署。SoundOutput 启动信号不能证明声卡/电台已实际辐射；PTT 状态来自应用收到的 transceiver 在线状态回读。若在线/PTT-off 回读一直未知，当前策略只记录并保留 DX，不会自动清理。

核心修复阶段报告、原始日志及本机候选的构建、测试、校验和回退记录均属于内部研发证据，不随运行安装包分发。
