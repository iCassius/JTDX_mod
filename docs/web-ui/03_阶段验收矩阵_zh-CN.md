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
| P6 | 全 API、异常隔离、UDP/CAT/解码/自动呼叫回归、浏览器结果、交付报告 | 进行中：页面确认取消/确认与 CQ pending→business readback 已人工观察；DX/频率及 CQ/Stop/AutoSeq 的软件合同已有自动证据；残留浏览器页清理阻塞全量 server 回归，断线/epoch/重复/timeout 浏览器场景、完整 MainWindow/CAT/PTT/TX/HIL 待后续 |
| P7 | 移除 token、精简页面文案、JTDX 结构化桌面/移动布局；保留 Host/Origin/CSRF、安全门和业务回读 | 已完成软件隔离验收：最终构建与 CTest `22/22` 通过；loopback 浏览器观察确认桌面左右布局、自动连接、无 token 输入、陈旧/操作结果提示；移动布局由静态契约覆盖 |
| P8 | 支持浏览器实际宽/窄视口、页面确认、业务回读、断线重连和横向溢出 | 进行中：`1280x720` 与 `390x844` 均有实际 DOM/截图证据；CQ 取消无操作记录，CQ/Stop/AutoSeq 完成回读，断线保留快照并可自动重连；在途 Stop 替换、DX 过期选择、浏览器异常身份/超时待后续批次 |
| P9 | DX 新鲜/过期选择、在途 Stop 接管、取消/重复 POST 计数、跨 epoch 与 timeout | 已完成隔离浏览器闭环：DX 匹配回读且不触发 TX；stale 锁定；Stop 以 `superseded_by_stop` 接管旧启动；取消/重复点击无额外 POST；跨 epoch 不误报完成；timeout 显示 `feedback_timeout`；定向/全量 CTest 通过 |
| HIL | CAT/PTT/TX、设备回读、长时运行和无线电行为；需独立授权 | 未授权 |

## 必测条目追踪

1. Web 启动、停止、异常；手动/自动 TCP 端口、占用；TCP 与 UDP 完全分离、无第二 UDP 监听、不改 UDP 配置。
2. 菜单重复点击、浏览器失败、设置保存重载、安全重启；状态 JSON、解码列表、SSE/重连、`Last-Event-ID`/revision、背压和连接上限。
3. 频率非法输入、TX 中安全语义、实际频率回读；DX 呼号/Grid 校验、过期 decode、独立 generation 回读、选择与开始分离。
4. CQ/AutoSeq 状态门、二次确认、停止优先级、业务 generation 回读；控制超时、重复 request、旧 epoch、revision 冲突、退出期间请求和未知状态提示。
5. 无 token 的 read/control、默认 loopback、显式 LAN 地址、Host/Origin/CSRF、body/连接/慢连接/日志限长；Web 异常不影响 JTDX。
6. 原有 UDP、CAT、解码、自动呼叫测试无回归；静态、单元、API 合同、浏览器、HIL、生产六层证据分别记录。

## 结果口径

每项证据记录命令、构建目录、提交、日志和未验证边界。HTTP 200、UDP datagram、accepted 或离线策略测试都不能证明控制完成、PTT 已动作或实际发射。没有真实设备时只报告软件层结果；HIL/部署必须另行授权。
