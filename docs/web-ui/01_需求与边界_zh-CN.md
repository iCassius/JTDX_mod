# JTDX 内置 Web UI：需求、边界与改进项

本文将用户需求编号为 `R01` 至 `R15`，后续实现和验收必须引用编号。当前均为设计基线，未表示代码完成。

## R01 目标与界面

在现有 JTDX Qt/C++ 程序内提供轻量 Web UI，查看状态、解码、DX、RX/TX 和运行结果，并支持有限的频率切换、DX 选择、CQ/自动呼叫启动和停止。前端使用原生 HTML/CSS/JavaScript，内置资源优先使用 Qt Resource；采用深色卡片、响应式布局、大字体频率/DX/RX/TX 和实时更新方向，不复制 TX-5DR 的代码、资源、CSS、图标或品牌。

页面分为顶部状态、RX、DX、TX、频率五区。RX 解码行只能选择 DX，选择与开始呼叫始终是两个动作。设置 Tab“Web UI/ Web 服务”包含启用、状态、TCP 端口自动/手动、仅本机/LAN、打开 UI、访问地址、端口冲突、重启、只读令牌/保护配置。View/Help 菜单“打开 Web UI”检查配置、确保唯一服务、用 `QDesktopServices::openUrl()` 打开浏览器并显示失败原因。

## R02-R05 网络、进程和数据边界

| 编号 | 约束 |
| --- | --- |
| R02 | Web 使用独立 TCP HTTP 端口，默认 `127.0.0.1:<web_port>`；启动只检查 TCP 冲突，绝不尝试占用 UDP 端口。 |
| R03 | 不改现有 UDP Server/Client 端口，不新增 UDP 监听，不用 UDP 转发 Web 控制，不启动 `message_aggregator`。 |
| R04 | Web 不新增 Gateway、Node/Python/Go 服务、守护进程或常驻线程；优先在现有 Qt 事件循环使用异步 `QTcpServer`，不为连接建线程。当前已有 `jtdxjt9` 解码子进程，Web 不新增、不删除、不重构它。 |
| R05 | 复用现有内部状态和 `MessageClient` 数据流；不重复解析 UDP，不从控件反向读取全部状态；Web 不直接依赖 `QLineEdit`/`QComboBox`。 |

LAN 只有用户明确开启才允许绑定；LAN 必须强保护并显示风险提示。Web 异常只能局部失败，不得影响音频、CAT、UDP、解码和既有自动呼叫。

## R06-R08 状态与控制边界

`JtdxWebState` 保存线程/事件循环安全状态快照、有限最近解码、更新时间、数据新鲜度、revision 和服务 epoch。解码建议默认保留 300 条、硬上限 500 条。`JtdxWebControl` 只把请求转换为现有频率、DX、CQ、AutoSeq、Halt/停止入口，操作完成依赖状态回读；不模拟点击、不直接写控件/PTT、不重写 TX 调度、不绕过 Enable Tx、安全检查和 watchdog。

频率操作提供频段、常用频率和手动输入，复用现有校验；TX 中遵循原有安全逻辑，实际 CAT/状态回读匹配后才完成。DX 只能来自当前有效解码或经过校验的合法输入，必须分离“选择”和“开始”。CQ/自动呼叫必须确认在线、模式、TX 安全门和允许状态，并由页面二次确认；停止优先走既有 Halt/AutoSeq 路径，不无条件强切 PTT。

当前源码中的 UI 槽函数和消息处理可能带有清 DX、Enable Tx、恢复票据等副作用，不能直接当 Web 业务入口。尤其不能把会触发自动发射的双击/`processMessage` 复用为“只选择 DX”；需要抽取最小只选择入口并验证所有 AutoTx/AutoSeq 组合不改变 Enable Tx。呼号输入先做长度/空值边界，再复用既有校验，避免短输入触发不安全访问。

## R09-R12 API 与结果口径

只提供：`GET /`、`/healthz`、`/api/v1/state`、`/api/v1/decodes`、`/api/v1/events`（SSE 或经论证的同等通道），以及 `POST /api/v1/control/frequency`、`select-dx`、`start-cq`、`start-auto-call`、`stop-auto-call`。不增加 Reply、FreeText、远程日志、音频、WebRTC、OpenWebRX 或通用远程控制。

所有控制均有 `request_id`、时间、epoch、初始 revision、截止时间、结果、失败原因和状态快照，结果使用 `accepted`/`pending`/`completed`/`failed`/`rejected`/`timeout`/`unknown`。HTTP 200、UDP 发送或 accepted 都不能表示已经完成或已经发射。非法输入返回 400，状态不允许/版本冲突返回 409，超时明确 pending/timeout；服务重启使旧操作失效且不自动重试。重复 request id 幂等，退出中拒绝控制。

## R13-R15 安全、测试与交付

读与控制分级鉴权；LAN 必须令牌/强保护。校验 `Host`/`Origin`，控制 POST 使用 CSRF 防护；限制 body、头/行、并发/单 IP 连接、慢连接、处理时间、SSE 队列、事件大小、日志长度。SSE 支持背压、断线重连和 revision/`Last-Event-ID` 重同步，无法补齐时要求全量 resync。

测试分为静态、单元、API 合同、浏览器手测、HIL、生产部署六层。必须覆盖启动/停止、自动/手动端口、占用、UDP 分离、重复菜单、设置保存重载、状态/解码/SSE、频率/DX/CQ/AutoSeq/停止、超时/重复/退出/冲突、异常隔离及原有 UDP/CAT/解码/自动呼叫回归。没有真实设备时不得声称验证 PTT、实际发射或无线电行为；HIL 和部署需独立授权。
