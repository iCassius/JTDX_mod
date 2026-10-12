# P033 CAT 长挂机与首发射修复说明

P033 首次把 CAT session 代次路由、首个 PTT 请求的真实读回就绪和取消边界、FTX-1 错序计数恢复、诊断日志字段完整性修复放入同一候选。P032 已交付的版本不包含这些 CAT 修复。

## CAT 离线和会话代次

配置层在 update/failure 上关联 rig generation。旧代 update 在缓存/UI 副作用前被忽略；当前 runtime 会话的 terminal offline 和 failure 向主窗口各传播一次。关闭当前会话不会让同一代的 failure 被推进后的新 generation 错误吞掉。离线 update 不把默认零频率当作有效调谐频率，也不触发 PTT-off halt。

## FTX-1 + CAT PTT 就绪门

仅 FTX-1 且采用 CAT PTT 的发送路径要求当前代次在线、状态属于当前代，并有请求开始后成功的真实 `rig_get_ptt()` serial 读回。对应 GET 操作期间新增协议错配时，该读回不建立 known 状态。`rig_set_ptt()` 成功回显只是软件侧状态变化，不能证明电台实际 PTT 已切换。

请求等待期间收到旧状态、相同 serial、旧代状态、PTT-off 或未知状态，不会启动音频。有效的新真实 PTT-on 读回只能为当前匹配请求 arm 一次音频；最终启动前再检查同一 token、代次与发送意图。没有引入自动 PTT-on 重试、固定 PTT TTL 或按 update 年龄判断失联。其他 backend 沿用原 readiness 来源，但尚未消费的请求同样接受取消与发送上下文保护。

## 取消与发送上下文

在音频开始前，关闭 Enable Tx/Tune、清空或替换 DX、手动 decode 接管、AutoSeq 目标改变、TX step/QSO 阶段、频率/band、mode/modeTx 变化等上下文更新会撤销旧 request/timer，并沿已有 sequencer 关闭 PTT。已经开始的发送周期仍按原有收尾路径处理。Tune 是独立发送意图。只有现有新鲜 AutoSeq 决策可以解除对应的 not-ready 拒绝；这不是新的定时重试机制。

内部安全 Halt Tx 关闭 Enable Tx 时，会保留软件拥有的初始自动 RCQ/RFIN owner。这样原有 120 秒停滞判定仍可在完整的新鲜 decode batch、当前真实 PTT-off 和其他安全条件满足后收尾。手动 DX、已进入 QSO 的目标及恢复 ticket 不被这条初始 owner 清理逻辑清掉。

## FTX-1 协议错序和诊断字段

每轮 poll 只有实际读操作成功且整轮无错配，才清除 protocol-sync 连续失败。没有有效 read 的轮次不会伪造成功，连续三次升级阈值和不安全 PTT 的 hard-failure 保持不变。日志字段独立编码/拼接，不让 `%20`、`%3A`、百分号、空格或中文内容被后续格式化参数重新解释。PTT 关键事件记录 request serial、generation、状态 generation、readback serial、pending/token/timer 状态和拒绝/取消/消费原因，不对无变化轮询重复刷日志。

## 故障反馈导出工具

P032 双击入口 `export-jtdx-diagnostics.cmd`、PowerShell 脚本、P032 导出指南仍随运行包安装；本版没有更改脚本接口或导出格式。既有合成测试覆盖本地日志/配置的筛选与 ZIP 导出。导出内容可能含呼号和操作时间，分享前请检查报告及筛选日志。工具不上传、不收集截图，也没有真实桌面人工验收。

软件构建、测试与本机 ZIP 完整性不是 CAT/PTT/TX/HIL 证明。本候选未启动程序、未连接真实电台、未发射或安装到用户目录。
