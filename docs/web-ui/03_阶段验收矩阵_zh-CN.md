# JTDX 内置 Web UI：阶段验收矩阵

状态只使用：`未开始`、`进行中`、`已通过`、`阻塞`、`未适用`。P0 文档通过不代表代码、浏览器、HIL 或生产通过。

| 阶段 | 范围和证据 | 当前 |
| --- | --- | --- |
| P0 | 需求 R01-R15、边界、改进项、恢复入口、源码/构建事实；`git show`/`git status`；不运行 JTDX | 已通过 |
| P1 | 状态快照、有限解码、更新时间/新鲜度、事件循环安全读接口；Qt/MinGW/CMake 独立探针；单元和无 UDP 新监听静态检查 | 已通过（本批独立 Ninja 构建 `[1218/1218]`、CTest `14/14`；未启动 JTDX/HIL） |
| P2 | TCP 启停、自动/手动端口、占用/UDP 分离、`/`、`healthz`、state、decodes、SSE 重连/背压 | 已通过（loopback/API/CTest；未启动 JTDX/HIL） |
| P3 | 设置保存/重载、LAN 开关和鉴权、菜单重复点击、资源、桌面/移动只读页面；浏览器手测 | 生产 `JtdxWebService`/真实 `QAction` 与 17 项自动测试已完成；完整 MainWindow 窗口人工验收留到 P6 |
| P4 | 频率校验/切换/实际回读、过期 decode、DX 校验/选择/应用；API 合同和单元 | 频率与 DX 软件安全门、独立显式配置、MainWindow 隔离适配契约、DX 专属 generation/source 回读、跨客户端未确认锁与 Service/Server/Control 生命周期 epoch 已通过隔离测试；完整窗口/CAT 回读、浏览器人工验收待后续批次 |
| P5 | CQ/AutoSeq 安全门、二次确认、启停状态机、超时/重复/退出/冲突；原有 AutoSeq/CAT 回归 | CQ 复用既有业务入口并由 Web 安全门打开 Enable Tx，Stop 复用既有停止入口后关闭 AutoSeq；AutoSeq 仍只验证“已启用、等待下一批实时解码”，软件隔离测试已通过；真实 CAT/PTT/TX/HIL 待后续 |
| P6 | 全 API、异常隔离、UDP/CAT/解码/自动呼叫回归、浏览器结果、交付报告 | 已由 P7-P11 分批完成有界软件/浏览器证据；完整 MainWindow 原生窗口、CAT/PTT/TX/HIL/部署仍未完成 |
| P7 | 移除 token、精简页面文案、JTDX 结构化桌面/移动布局；保留 Host/Origin/CSRF、安全门和业务回读 | 已完成软件隔离验收：最终构建与 CTest `22/22` 通过；loopback 浏览器观察确认桌面左右布局、自动连接、无 token 输入、陈旧/操作结果提示；移动布局由静态契约覆盖 |
| P8 | 支持浏览器实际宽/窄视口、页面确认、业务回读、断线重连和横向溢出 | 已完成有界浏览器验收：`1280x720` 与 `390x844` DOM/截图、确认、断线重连和无横向溢出；真实 MainWindow 视觉未验 |
| P9 | DX 新鲜/过期选择、在途 Stop 接管、取消/重复 POST 计数、跨 epoch 与 timeout | 已完成隔离浏览器闭环：DX 匹配回读且不触发 TX；stale 锁定；Stop 以 `superseded_by_stop` 接管旧启动；取消/重复点击无额外 POST；跨 epoch 不误报完成；timeout 显示 `feedback_timeout`；定向/全量 CTest 通过 |
| P10 | 错误身份回包、确认后重复提交、MainWindow 无硬件隔离审查、软件交付报告 | 已完成软件收尾：错误 `request_id`/`server_epoch` 回包均保持未知/处理中保护；同一确认 POST 实际重复转发仍只保留一条记录；自有 `--test-mode` MainWindow 安全启动/停止通过；原生窗口人工验收、CAT/PTT/TX/HIL/部署未完成 |
| P11 | 主程序嵌入资源重建、未确认锁后的安全 Stop、结果字段和 MainWindow 集成边界 | 已完成最小 operation-specific Stop 修复：超时后 Stop 以 `automation_stopped`/`idle/disabled` 回读完成但不解除旧 latch；主程序 `qrc_jtdx.cpp`/`jtdx.exe` 已重建，最终 CTest `22/22`；原生窗口人工验收、CAT/PTT/TX/HIL/部署未完成 |
| P12/P13 | 有界本地控制诊断日志、Hamlib 同文件统一写入、超大既有文件边界、生命周期/重复/拒绝记录、P11 新 epoch/多浏览器恢复核对 | 已完成：复用现有数据目录的 `jtdx_recovery.log`，主动文件 `256 KiB`、仅 `.1` 轮转；保留既有本地时间/UTF-8/`key=value` 约定，定向 CTest `6/6`、全量 CTest `23/23`；普通控制恢复步骤已明确为必要时重启 JTDX；原生窗口人工验收、CAT/PTT/TX/HIL/部署未完成 |
| P14 | 本地 Release Candidate：CMake 安装、Qt/MinGW/Hamlib 依赖闭包、ZIP 直根目录、SHA256、干净解压和无硬件启动 | 已完成有界 RC 审计：75 文件、`bin/plugins/share` 直根目录、逐文件解压比对 0 差异、两个目录各观察 20 秒；完整 MainWindow、无硬件长时/故障恢复、CAT/PTT/TX/HIL、最终发布批准仍待人工或另行授权 |
| P15 | 无硬件持续运行、受限 SSE/慢客户端/连接上限、周期重连、未决命令 Stop 接管、stale 安全拒绝、资源采样 | 已完成有界 fixture 证据：1802 秒、29 次采样、15 次恢复周期、连接上限第 17 个 `503`、慢客户端有界清理、fixture 自然退出 `0`、无意外失败；仅证明 State/Control/Server 隔离链路，不替代 RC MainWindow、CAT/PTT/TX/HIL |
| P16 | 解压 RC 的真实 `jtdx.exe` 无硬件持续运行、配置隔离、安全默认值、主进程资源/网络观察和最终交接 | 已完成有界 RC 证据：1814.2 秒、30 次采样、私有内存 `70.55–70.67 MB`、句柄 `237–239`、网络 `0`；受控终止，正常 MainWindow 关闭、设置保存重载、完整视觉、CAT/PTT/TX/HIL 和最终发布批准仍待人工或授权 |
| P17 | 截图对应的 WebUI 菜单、紧凑解码表、受限电台面板、Tx1–Tx6 编辑、能力开关、操作回读和新 RC | 软件实现与定向测试已完成；夹具宽屏已观察到解码一行布局、国家字段、面板、Tx6 选择和有界滚动；真实 MainWindow、CAT/PTT/TX/HIL、部署和最终发布批准仍未完成 |
| P18 | 截图差异全表、固定 radio API/QSO 参数、Web Log QSO 草稿/确认/取消/重复保护、1280/390 实际视口、菜单/服务回归和最终构建 | 已通过隔离软件验收；QSO fixture 不写 ADIF；真实 MainWindow、CAT/PTT/TX/HIL、部署和最终发布批准仍未完成 |
| P19 | 真实 LogQSO 文件追加、失败不报成功、隔离主程序拒绝、RC 与临时物清理 | 已完成隔离真实 LogQSO 与 `24/24` 全量 CTest；真实 DX 成功确认、request_id 重试和 HIL 仍未验证 |
| HIL | CAT/PTT/TX、设备回读、长时运行和无线电行为；需独立授权 | 未授权 |

## 必测条目追踪

1. Web 启动、停止、异常；手动/自动 TCP 端口、占用；TCP 与 UDP 完全分离、无第二 UDP 监听、不改 UDP 配置。
2. 菜单重复点击、浏览器失败、设置保存重载、安全重启；状态 JSON、解码列表、SSE/重连、`Last-Event-ID`/revision、背压和连接上限。
3. 频率非法输入、TX 中安全语义、实际频率回读；DX 呼号/Grid 校验、过期 decode、独立 generation 回读、选择与开始分离。
4. CQ/AutoSeq 状态门、二次确认、停止优先级、业务 generation 回读；控制超时、重复 request、旧 epoch、revision 冲突、退出期间请求和未知状态提示。
5. 无 token 的 read/control、默认 loopback、显式 LAN 地址、Host/Origin/CSRF、body/连接/慢连接/日志限长；Web 异常不影响 JTDX。
6. 原有 UDP、CAT、解码、自动呼叫测试无回归；静态、单元、API 合同、浏览器、HIL、生产六层证据分别记录。
7. P17 电台面板仅允许白名单 action；危险操作需确认；`Log QSO` 只打开原生记录对话框，不能由 WebUI 伪造 ADIF 完成。
8. P18 Web Log QSO 先进入可编辑临时草稿；确认后复用现有记录模型并等待关闭草稿/generation 回读；未知 QSO 字段、空白危险确认、重复键和迟到回读不得误报完成。

## 结果口径

每项证据记录命令、构建目录、提交、日志和未验证边界。HTTP 200、UDP datagram、accepted 或离线策略测试都不能证明控制完成、PTT 已动作或实际发射。没有真实设备时只报告软件层结果；HIL/部署必须另行授权。
