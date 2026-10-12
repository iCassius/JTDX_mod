# P034 CAT 与 PTT 状态反馈指南

本指南适用于 P034 本机审阅候选。它帮助收集 CAT/PTT 状态和故障反馈，不代表本机软件检查已证明实际串口或电台行为。

## CAT PTT 状态观察

状态栏的 CAT PTT health observer 只对当前活动、支持观察的 rig session 接收 poll/GET 结果。显示 `suspect` 表示观测证据超出策略窗口或 poll 长时间未完成；其中 5 秒长 poll 阈值只是观察性 suspect 标记，不是 PTT readiness 超时，也不会自动重连、关闭串口或改变发射控制。

`readFreq` 保留原有同步动作，再在 rig 当前在线时单独请求一次 PTT GET：先调用 `sync_transceiver(true)`，再请求只读刷新。这个刷新请求本身不会打开/重开 rig 或执行 CAT 控制写入；旧同步动作仍使用头文件默认 `enforce_mode_and_split=false`，因此可能保留原有 split-off/TX 频率清零兼容行为。请勿把整个按钮称为完全无副作用的纯 GET。

FTX-1 CAT 的 PTT 状态只有在当前会话的成功 GET 和协议同步检查都通过时才进入已知状态。Hamlib `RIG_OK` 与 protocol guard 通过不等同于已证明串口线上往返或电台响应；本 observer 不诊断 COM 长时间占用/释放根因。其他后端可能沿用 legacy 状态来源，来源标签不代表真实硬件 GET。

## 待发 PTT 的取消边界

等待音频启动的请求绑定当前 rig generation、发送上下文和实际选中的 TX1–TX8 payload。编辑当前选中 payload 会使旧 token 失效；修改未选中的消息不会取消请求。Tune 和独立 WSPR generator 保持各自路径。retired rig 对象中的排队命令会被拒绝；新 rig session 才能建立新的命令授权。

PTT admission 在 worker 接纳和调用 driver 前设有撤销检查，能阻止尚未进入 driver 的旧 queued PTT-on。已经进入具体 driver 的调用无法被撤回，因此不得把软件 cancel 当成能中断任意已开始的设备调用。unknown/offline 等安全状态保持 fail-closed。

## AutoSeq 与 DX 保留条件

AutoSeq 初始 owner 的 120 秒无实际进展规则仍需经过完整安全门：完整新鲜 decode batch、目标 context 匹配、rig 在线、目标选择之后的已知 PTT-off，以及没有 pending、recovery 或 operator hold 等阻挡条件。unknown、offline、PTT-on、ticket 或缺少新鲜 batch 时会保留 owner/DX；手动 DX 与已进入 QSO 的目标受保护。此行为优先保留目标，不承诺自动清除所有停滞状态。

状态观察时间刷新不等于发射进度。strict GET 需在当前代次递增 serial；legacy 在线 callback 可刷新观察时间。AutoSeq 实际 progress 仍要求匹配请求的 PTT off→on 状态边沿（边沿来源按后端区分）或既有音频启动确认。

## 导出和反馈

在实际安装目录中确认版本为 P034，然后双击该目录下的 `bin\\export-jtdx-diagnostics.cmd`。按提示输入故障发生的本地时间（`yyyy-MM-dd HH:mm[:ss]`；回车表示现在）；工具默认收集此前 30 分钟至此后 10 分钟，并把 ZIP 保存到桌面，完成时会打印完整文件路径。请打开 ZIP 中的 `report.txt` 核对摘要。若手头记录的是 UTC，请先换算为电脑本地时间输入，并在反馈中同时注明原始 UTC、时区和本地时间；命令行高级时间窗和输出路径见 [P032 故障反馈导出指南](P032_故障反馈导出指南_zh-CN.md)。工具只在本机读取有限日志窗口与配置白名单，不自动上传。

反馈请提供实际运行版本、故障 UTC/本地时间及时区、界面截图和刚生成的 ZIP；截图另作附件。JTDX.ini 安全白名单摘要不含端点字段、凭据或原始 `@Variant`。`ptt_request_*` 与 `ptt_audio_start_refused` 专用事件不写原始 TX payload 或 payload identity，但可能含呼号/目标上下文；窗口内恢复日志和 ALL.TXT 原行仍保留，可能包含消息或 TX 文本。端点是否实际出现在原始日志中尚未核实，不能保证整份 ZIP 不含端点或 TX 文本。分享前请检查 report 与全部条目；通用反馈表见 [A0–A5 故障反馈指南](A0-A5_故障反馈指南_zh-CN.md)。`@Variant` 可能显示“已保存但无法安全解码”，这不等于配置未保存；不要通过摘要推断现场根因。

截至 P034 的历史 `004430` 与 `052800` 片段缺少足够的代次、请求和读回关联信息，不能确认是同一故障。反馈时尽量保留完整时间窗口和对应版本/来源字段；排查结果仍需与真实设备证据分开记录。

## 验证边界

本机 Release build、CTest、fake worker、策略测试、合成导出和 ZIP 校验不是完整 MainWindow GUI、真实 CAT/PTT/TX、COM 释放、HIL 或 `159` 验收。最终 P034 候选报告记录逐项命令、实际测试数量、ZIP 大小与 SHA-256。

## 2026-10-05 A1 readFreq后续校正

上文关于 `readFreq` 沿用 `sync_transceiver(true)`、并保留默认参数 split-off/TX频率清零行为的说明有误；该历史文字保留作审计记录。A1恢复基线原行为：先保留 `transceiver_online()` 门控，在在线分支调用 `sync_transceiver(true, true)`，再单独请求一次只读 CAT refresh。offline 不执行 sync 或 refresh；没有新增自动连接/重连。

A1针对性测试在本机编译了从当前 `mainwindow.cpp` 与 `Configuration.cpp` 提取的实际函数体；fake adapter 捕获单参数调用触发的 `transceiver_tx_frequency(0)` 负例，并覆盖 online、offline、transmitting 三条路径。两项目标测试通过。该证据不是完整GUI或真实CAT/PTT/TX验证；详细 A1 记录属于内部研发资料，不随运行安装包分发。

## 2026-10-06 A4 隐私边界校正

上文原有“端点/主机/凭据及 PTT 请求 TX 文本不导出”的表述把配置摘要及专用事件的限制扩大到了整个 ZIP，现已由上方说明和 [通用反馈指南](A0-A5_故障反馈指南_zh-CN.md) 校正。此条保留 P034 历史文档事实，不改变当时软件/候选的验证结论。
