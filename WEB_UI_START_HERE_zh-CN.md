# JTDX 内置 Web UI：阶段入口与恢复说明

## 当前候选：P26 Web 电台控制超时与六按钮收口（2026-09-24；待根任务复核）

P26 基于 `main/26cf365`，最终实现提交 `b0732b9`。除修复 Radio dispatch 重复 prepare/begin 导致原生动作未执行的问题及补齐电台操作嵌套回读外，也覆盖 TX 开关回读一致性和 430px 窄屏宽度约束。完整 Release 构建成功、全量 CTest 26/26；视口量测、截图、六项可见电台控件、QSO 草稿边界、超时安全锁、审阅包及未验证范围见 [`docs/web-ui/P26-Web电台控制超时与六按钮收口_zh-CN.md`](docs/web-ui/P26-Web电台控制超时与六按钮收口_zh-CN.md)。最新审阅 ZIP、校验和与短说明位于 `C:\JTDX64` 根目录。等待根任务复核，不代表验收或发布批准。

浏览器验证只使用本机 loopback 内存夹具。未启动真实 MainWindow，未连接/操作 CAT/PTT/TX、未发射、未做 HIL、UDP 服务线程、LAN/公网或部署验证；不得把本机审阅候选解释为公开发行包。

## 当前候选：P25 解码与安全门纠偏（2026-09-23；待根任务复核）

根任务尚未接受此前阶段。P25 修正解码投影/地理字段、解码卡选择关联及 Web 动作级电台安全门，源码提交 `05ee60d5de8ae1d29b00df6d23badcf12cd435c6`。构建、25 项全量测试、三种视口、141 文件安装包与 SHA-256、未验证边界见 [`docs/web-ui/P25-解码地理字段与电台安全门纠偏_zh-CN.md`](docs/web-ui/P25-解码地理字段与电台安全门纠偏_zh-CN.md)。当前 ZIP 交付位置为 `C:\JTDX64` 根目录；今后所有新 Release ZIP 也直接输出到该目录，历史 ZIP 留在各自归档位置。该阶段等待根任务复核，不代表验收或发布批准。

P25 浏览器证据来自本地 loopback 内存夹具；未操作 CAT/PTT/TX、未发射、未做 HIL。真实 MainWindow、LAN/公网、部署和第三方法律清权仍未验证/完成。不得把 P25 包解释为公开发行包。

## 前一候选：P24 根审阅纠偏

P24 是历史本机候选，旧构建与包证据见 [`docs/web-ui/P24-根审阅纠偏_zh-CN.md`](docs/web-ui/P24-根审阅纠偏_zh-CN.md)；其候选状态不代表根任务接受。P25 是后续当前源码状态。

## 历史阶段：P23（2026-09-23；已由 P24/P25 后续纠偏覆盖）

P23 文档记录该阶段当时的设计和验证，不再作为当前视觉行为说明；P24 的有限布局/文案/校时纠偏见上方入口及 [`docs/web-ui/P24-根审阅纠偏_zh-CN.md`](docs/web-ui/P24-根审阅纠偏_zh-CN.md)。P23 当时关于 Web 专属门控调整、原生业务安全/未知状态/幂等/回读、loopback 默认监听等行为记录仍作为历史背景，当前控制和安全边界不得仅凭旧阶段文字推断。

本批实现、前后端映射、测试与浏览器证据、最终本地安装包哈希和未验证边界见 [`docs/web-ui/P23-电台状态紧凑布局与直接操作_zh-CN.md`](docs/web-ui/P23-电台状态紧凑布局与直接操作_zh-CN.md)。较早章节是其当时状态的历史记录；不得将其旧默认关闭/确认/访问限制表述为 P23 当前行为。真实 CAT/PTT/TX、HIL、部署和发布权利审查仍未因此获得验证或授权。

本目录记录 JTDX 内置轻量 Web UI 的需求、边界、设计和阶段证据。P3 生产 service/QAction、P4 普通控制、P5 CQ/AutoSeq 软件控制链路、P7 页面收口、P8 视口/断线验收、P9 DX/异常闭环、P10 软件交付收尾、P11 超时 Stop 恢复/资源核查、P12/P13 有界本地控制诊断日志、P14 本地 RC、P17/P18 Web 电台面板与 QSO 草稿及 P19 持久化修复已完成对应的软件验证；P20 进一步区分启动安全门拒绝原因并准备本地候选包。完整 MainWindow Web QSO 成功链仍未做隔离集成验收，真实 CAT/PTT/TX、HIL 和部署也未验证。频率、DX、CQ/AutoSeq 控制可由用户在设置中分别显式开启，默认仍关闭；完成仍须实际业务/CAT 状态回读，不能把隔离夹具的 `accepted/pending` 当作硬件完成。较早 CAT `.10` 交付提交 `a89c9da` 仅作外部历史参考。

## P19 Web QSO 真实持久化（2026-09-21）

P19 修复 Web QSO 初始化误写两条 ADIF 和写入失败仍报成功的问题：初始化只填充草稿，显式确认实际写入成功后才清空草稿并推进 generation。真实 `LogQSO` 隔离测试确认一次确认一条 ADIF、取消零写入、不可写路径返回失败且不写业务日志；真实 `jtdx.exe --test-mode --rig-name P19-Web-Persist-9677` 仅验证本地 HTTP 拒绝与无文件副作用，未伪造无 DX 条件下的成功通联。详见 [`docs/web-ui/P19-Web-QSO真实持久化与清理_zh-CN.md`](docs/web-ui/P19-Web-QSO真实持久化与清理_zh-CN.md)。

## P20 启动拒绝原因与本地候选交接（2026-09-23）

`m_start2` 是首个电台状态回读前的启动门，不代表 TX/PTT 已活动；相应拒绝现在返回 `startup_pending`，既有启动/发射安全门不变。P20 的 `jtdx_web_control_test`、`logqso_persistence_test` 与 `jtdx_web_server_test` 均为独立组件测试，不覆盖真实 MainWindow dispatch 到 ADIF 的同一集成调用链。当前本地 Release 候选、哈希、回归证据、回退包和人工/HIL边界见 [`docs/web-ui/P20-真实LogQSO集成边界与候选发布_zh-CN.md`](docs/web-ui/P20-真实LogQSO集成边界与候选发布_zh-CN.md)。

## P14 本地 Release Candidate（2026-09-21）

已生成隔离 RC 目录和 ZIP，直接根目录为 `bin/plugins/share`，75 个文件，ZIP SHA256 为 `776df727a46242d65a1bdf1908fd86ac98019b39fb9935d7aadd1c28000e70cc`。候选包和全新解压目录均以 `--test-mode` 无硬件启动；P15 完成 1802 秒 State/Control/Server fixture 长时证据，P16 又用解压 RC 的真实 `jtdx.exe` 完成 1814.2 秒主程序无硬件存活/资源证据，但结束为受控终止，不等于正常 MainWindow 关闭。完整原生设置/视觉/保存重载、真实 CAT/PTT/TX/HIL、发布批准仍未完成或未授权。详见 [`docs/web-ui/P14-Release候选审计_zh-CN.md`](docs/web-ui/P14-Release候选审计_zh-CN.md)、[`docs/web-ui/P15-无硬件持续运行与故障恢复_zh-CN.md`](docs/web-ui/P15-无硬件持续运行与故障恢复_zh-CN.md) 与 [`docs/web-ui/P16-RC主程序无硬件持续运行与最终交接_zh-CN.md`](docs/web-ui/P16-RC主程序无硬件持续运行与最终交接_zh-CN.md)。

## 最新恢复结果（2026-09-21）

### P12 有界本地控制诊断日志（2026-09-21）

控制生命周期写入现有数据目录下的 `jtdx_recovery.log`：主动文件最多 `256 KiB`，只轮转为一个 `.1` 文件，沿用既有本地 ISO 毫秒时间和 UTF-8；目录、文件名和轮转均沿用现有实例隔离路径。记录只保留规范化 request ID、操作、事件/状态、限长原因、generation/state revision 和有限 readback 摘要；Web 字段值的换行、制表符、反斜杠和等号会被安全处理，通用写入器保留既有 `key=value` 分隔，绝不记录 Web Authorization、token、密码、原始 HTTP 或凭据。日志写失败不改变控制和安全结果，日志也不参与未知状态恢复。

P11 的未确认锁经核对仍是服务级安全状态：新 epoch、多浏览器、断线重连、停止/启动 Web 服务和日志文件都不能解锁；只有当前 Stop 经当前 epoch/revision 校验并得到 `idle/disabled` 回读后才完成停止，而且旧未知锁继续保留。若 Stop 已确认但普通 CQ/AutoSeq/频率/DX 控制仍被锁定，必须关闭并重启 JTDX，让新的 `JtdxWebControl` 实例重建；不能靠刷新网页、重启 Web 服务、读取/删除日志或自动重发绕过安全门。最终代码为 `d6dbad6`、`a19146f`、`dc62904`；构建见 `C:\JTDX64\deps-webui\p13-final-build-20260921.log` 与 `p13-ui-build-20260921.log`，定向测试见 `p13-focused-20260921.log`，全量 CTest `23/23`、`100% tests passed`、57.48 秒见 `p13-final-ctest-dc62904-20260921.log`。真实 MainWindow、CAT/PTT/TX/HIL 和部署仍按边界单独报告。

### P7 Web 访问与页面布局收口（2026-09-21）

Web UI 已移除 token 访问保护及前端令牌输入；旧 `WebUiTokenSha256` 配置键不再显示、写回或参与服务签名。默认 loopback、显式 LAN 地址、Host/Origin/JSON/CSRF 边界、控制默认关闭、确认、幂等、超时、业务 generation 和实际回读均保持不变。页面改为自动连接/断线重连，桌面按“左侧 RX/解码/操作、右侧频率/DX/TX/CQ/AutoSeq”排列，窄屏按左侧后右侧纵向排列；删去开发合同、epoch/revision 等实现性页面文案，保留未知/陈旧/断线和“HTTP 登记不等于完成”的提示。

本批使用无硬件 loopback fixture 和受支持浏览器观察页面；未处理既有浏览器残留页，不启动真实 `jtdx.exe`，不连接 CAT/PTT/TX，不做 HIL、部署或真实无线电验证。最终构建日志为 `C:\JTDX64\deps-webui\p7-final-build-20260921.log`，全量 CTest 为 `22/22`、`100% tests passed`，日志为 `C:\JTDX64\deps-webui\p7-final-ctest-20260921.log`。浏览器观察到桌面左右布局、自动连接、无令牌输入和陈旧/操作结果提示；移动窄屏以静态契约覆盖，未声称真实 MainWindow 视觉验收。

### P8 支持浏览器验收闭环（2026-09-21，有界收尾）

使用本批自有 `--serve-browser-automation-p6` loopback fixture 和受支持的 Codex In-app Browser 完成实际视口验收。默认视口 `1280x720` 的 `workbench` 实测宽度 `1064`，左/右工作区各 `525`，`documentElement.scrollWidth=clientWidth=1265`；显式 `390x844` 窄屏实测 `innerWidth=390`、`clientWidth=scrollWidth=375`，左工作区位于顶部、右工作区位于其后，均无横向溢出。页面截图已在本批浏览器会话中采集。

浏览器业务证据：页面内确认框为 `role=dialog`，默认焦点为取消；取消 CQ 后操作记录保持为空。确认后 CQ 记录为“已完成（已确认）/cq_armed”，fixture 呈现模拟活动 TX，Stop 可用；Stop 经“命令已登记，等待业务状态回读”后成为“已完成（已确认）/automation_stopped”；AutoSeq 同样完成业务回读。窄屏操作记录保留 3 条，记录区宽度 `313` 且没有额外横向滚动；停止自有 fixture 后页面显示未连接并保留旧记录，重启同一 fixture 后自动恢复已连接。

本批额度在收尾检查时五小时窗口剩余约 `29%`，按规则停止新实现。因此未扩展 fixture：尚未完成“启动在途时被 Stop 取代”的专项浏览器时序、DX 有效/过期选择、浏览器级超时/身份不匹配和 POST 次数网络计数；它们不被本批宣称为已验收。上述浏览器证据仍只代表隔离软件链路，不代表完整 MainWindow、CAT/PTT/TX、HIL 或生产部署。

### P9 DX、在途停止与异常状态浏览器闭环（2026-09-21）

新增仅测试参数 `--serve-browser-automation-p9`（49154）和 `--serve-browser-automation-p9-timeout`（49155），均复用生产 State/Control/Server 路径，不新增生产 API、线程、UDP 或调试端点。新鲜解码选择实际只产生 1 个 `select-dx` POST，回读 `K1ABC/FN31`、`feedback_matched`，TX 允许/发射仍为否；fixture 进入 stale 后 DX 选择按钮消失、状态显示陈旧，不再发送选择请求。

AutoSeq 在页面确认后保持 pending，Stop 在其回读前确认并只产生 2 个业务 POST；Stop 以 `already_selected` 完成，旧 AutoSeq 以 `superseded_by_stop` 拒绝，旧回调未复活状态。取消 CQ 和重复点击确认流程均为 0 个 POST、无新增操作记录。pending 请求断开并重启 fixture 后显示旧请求已失效，不误报完成；timeout fixture 以 1 个 POST 经 pending 后显示 `feedback_timeout`，按钮恢复可用。

P9 构建日志为 `C:\JTDX64\deps-webui\p9-fixture-build-20260921-r9.log`，服务定向 CTest 为 `1/1`、34.34 秒，日志为 `C:\JTDX64\deps-webui\p9-server-focused-20260921.log`；全量 CTest 为 `22/22`、57.51 秒，日志为 `C:\JTDX64\deps-webui\p9-final-ctest-20260921.log`。仍未启动真实 `jtdx.exe`，未连接 CAT/PTT/TX，未做 HIL、生产部署或完整 MainWindow 验收。

### P10 软件交付收尾与 MainWindow 隔离审查（2026-09-21）

P10 用临时本机 TCP 代理做浏览器测试流量故障注入，不改 fixture 内存、不绕过页面确认；错误 `request_id`、错误 `server_epoch` 回包均使页面保持“结果未知/处理中”，不误报完成。真实确认后的同一 `start-auto-call` POST 被代理转发两次，服务端和页面仍只保留一条 `处理中/awaiting_feedback` 记录；`C:\JTDX64\deps-webui\p10-proxy-duplicate.stdout.log` 记录 `DUPLICATE_FORWARD`/`DUPLICATE_COMPLETE`。超时后前端保持未知锁，Stop 被服务端以 `unconfirmed_feedback` 拒绝时仍不重新开放启动按钮；最终定向 `3/3`、全量 `22/22`。临时代理脚本、fixture、代理和浏览器页均已清理。

已审查并安全启动自有 `C:\JTDX64\build-webui-dev-msys2\jtdx.exe --test-mode --rig-name p10-mainwindow`，确认使用独立测试实例锁、默认 `Rig=None`/UDP 控制关闭/Web 关闭边界，观察到的 `jtdxjt9` 是既有解码子进程；自有进程随后已停止，未终止用户实例，未连接 CAT/PTT/TX。当前工具没有原生 MainWindow 可操作面，因此设置 Tab、保存重载、菜单重复、端口冲突/重启/退出释放和完整窗口视觉仍不宣称已验收。详见 [`docs/web-ui/P10-软件交付报告_zh-CN.md`](docs/web-ui/P10-软件交付报告_zh-CN.md)。

### P11 超时 Stop 恢复与交付边界核查（2026-09-21）

P10 首次观察到“超时后 Stop 被 `unconfirmed_feedback` 拒绝”；源码审查确认这是全局 latch 误拦截既有 fail-safe Stop 的缺口。P11 只允许 `StopAutoCall` 继续进入既有停止入口，启动/频率/DX 等改变操作仍保持未确认锁；Stop 必须以 `idle/disabled` 业务回读完成，完成也不解除旧 latch。更新后的浏览器 fixture 先显示未确认 `AutoSeq enabled/armed/calling`，超时后确认 Stop，实际得到 `automation_stopped`、`idle/idle`、AutoSeq `disabled`，启动按钮仍禁用。

主程序资源链已实际重建：`p11-build-20260921-r3.log` 显示 `qrc_jtdx.cpp` 重新生成并链接 `jtdx.exe`；fixture Server 资源的最终链接见 `p11-build-20260921-r5.log`。最终全量 CTest 为 `22/22`，日志 `C:\JTDX64\deps-webui\p11-final-ctest-20260921.log`。`configuration_web_ui_test` 已自动覆盖真实 Configuration 对话框取消/确认、保存重载和安全默认值；完整 MainWindow 人工步骤、真实 CAT/PTT/TX/HIL/部署/LAN 仍未验证。

### P6 页面确认与隔离浏览器业务回归（2026-09-21）

本批将 CQ/AutoSeq/Stop 的浏览器二次确认从原生 `globalThis.confirm()` 改为页面内可访问对话框：`role=dialog`、`aria-modal=true`、标题/说明关联、默认焦点为“取消”、Escape 取消、Tab 在取消/确认之间循环。取消在生成 `request_id` 和发送 POST 之前结束，因此不产生操作记录；服务端仍强制要求 `confirm=true`，没有放宽控制门。操作卡补充 CQ、AutoSeq、Stop 的中文名称。

loopback 浏览器人工证据：连接隔离夹具后，点击 CQ 出现页面确认框且焦点位于“取消”；取消后操作结果仍为空；再次确认后先显示“命令已登记，等待业务状态回读”，随后显示 `CQ/AutoSeq 操作已由业务状态回读确认`，操作卡为 1 条、原因 `cq_armed`。这证明的是隔离软件链路，不是 MainWindow、CAT、PTT 或真实发射。原先由原生确认框卡住的旧标签页无法由当前 Computer Use 安全层清理，后续服务测试的 `slow active count` 失败归类为残留浏览器 SSE 环境污染，不作为源码回归。

自动化/构建证据：页面静态契约和资源重建见 `C:\JTDX64\deps-webui\p6-dialog-fixture-build-20260921.log`；`mainwindow_web_frequency_contract_test` 通过见 `C:\JTDX64\deps-webui\p6-dialog-test-fix-focused-20260921.log`。受残留浏览器页影响的回归日志 `C:\JTDX64\deps-webui\p6-dialog-fixture-focused-20260921.log` 保留用于环境说明；此前同一批 `jtdx_web_server_test` 在无该残留连接时通过，但本批不把它当作清理后的最终全量证据。新增 `--serve-browser-automation-p6` 仅用于后续无旧客户端的临时端口隔离夹具，不是生产端点。

尚未在本批浏览器人工验证：AutoSeq 确认完成、Stop 活动 TX/PTT 的页面回读、重复点击、断线重连、epoch/身份不匹配和超时；这些由现有 Control/Server 自动合同及后续可控浏览器会话继续补验。AutoSeq 仍是 decode-driven，等待实时解码，不等于立即呼叫当前 DX。

### P4 DX 选择软件链路批次

新增独立 `WebUiDxControlEnabled` 配置与 `POST /api/v1/control/select-dx` 路由。请求只携带当前 `decode_id`，服务端按有界新鲜实时解码重新校验 Call/Grid；MainWindow 只更新 DX 输入投影，不进入双击、QSO、标准消息、AutoSeq 或 TX/PTT 路径。`JtdxWebState` 发布独立 `dx_generation`、来源、解码 ID、DF 和时间，完成仅接受匹配回读。新鲜解码行提供“选择 DX”按钮，未知响应保持锁定。

本批自动验证覆盖纯校验、状态选择、Control、Server/Service、配置持久化、MainWindow 副作用静态契约及 `node --check`；未启动真实 JTDX，未连接 CAT/PTT/TX，未做 HIL、完整窗口人工验收、浏览器人工验收或部署。

### P5 CQ/AutoSeq 启停软件链路批次

新增默认关闭的 CQ/AutoSeq 能力开关和三类命令。页面每次要求二次确认，主程序安全门、业务 generation 和 `cq_state/auto_sequence_state` 回读共同决定完成；停止保持既有停止优先级。该批只证明软件状态机合同，不证明 CAT、PTT 或真实发射。

### P5 启停语义审查与最小修复（2026-09-21）

本批针对“页面请求是否真正进入主程序业务入口”做了源码链路复核并修复三处语义缺口：`start-cq` 复用既有 `on_txb6_clicked()` CQ 入口，Web 专用适配在安全门通过后再调用 `enableTx_mode(true)`；`start-auto-call` 明确命名为“启用 AutoSeq”，只打开既有、由下一批实时解码驱动的 AutoSeq，不凭空生成目标、不直接调用 `process_Auto()`；`stop-auto-call` 先复用既有停止入口，再关闭 `m_autoseq`，并允许停止优先于活动 TX/PTT 安全门。已有未完成的启动请求可被停止请求安全地标记为 `superseded_by_stop`。

受影响代码、Control/Server/高层适配契约测试及 loopback 浏览器夹具已重建。`jtdx_web_control_test` 与 `mainwindow_web_frequency_contract_test` 为 `2/2`，`jtdx_web_server_test` 为 `1/1`；浏览器隔离夹具确认启用业务能力后显示 CQ/AutoSeq 控件，并实际出现启动二次确认文本。由于原生确认框在本次浏览器控制会话中阻塞了自动化控制，未把“取消确认后没有 POST”写成已证明事实；该项仍需后续可控浏览器会话补验。此次没有运行真实 `jtdx.exe`，没有 CAT/PTT/TX/HIL 或部署证据。

### P4 频率恢复边界与显式启用批次

已完成 MainWindow 频率适配的隔离契约测试和真实 TCP 跨客户端未确认锁测试。配置 Tab 的频率控制复选框恢复为可由用户显式开启，专用键 `WebUiFrequencyControlEnabled` 默认仍为 `false`；MainWindow 不再硬编码关闭，而是把该配置值交给 Web 服务。实际 dispatch 仍在主 Qt 事件循环中重新读取 Rig/monitor/TX/PTT/IPTT/tune/AutoTx/状态新鲜度，复用 `band_changed()`，并只接受后续真实 rig generation/频率回读。`Rig=None`/CAT 离线隔离测试在 dispatch 前拒绝，未调用 TX/PTT 入口。

本批构建目标退出码为 `0`；定向 CTest `5/5` 通过；最终完整 CTest `21/21`、`100% tests passed`、57.25 秒。权威日志为 `C:\JTDX64\deps-webui\p4-frequency-gate-final-ctest.log`，定向日志为 `C:\JTDX64\deps-webui\p4-frequency-gate-focused-ctest.log`。未启动真实 JTDX，未连接 CAT/PTT/TX，未做 HIL 或部署。该批只完成软件安全门和配置流程，不等于真实设备验证。

### P4 频率候选边界浏览器验收（2026-09-21）

已在隔离 loopback 夹具补齐空候选、上下文变化、候选失效、手动文本保留、选择不自动发送和断开状态验收。测试专用参数为 `--serve-browser-frequency-empty` 与 `--serve-browser-frequency-invalidated`，不新增生产端点或控制能力。清理浏览器页和夹具后，受影响服务端测试 `1/1` 通过；最终完整 CTest `20/20`、`100% tests passed`、57.18 秒，日志为 `C:\JTDX64\deps-webui\p4-frequency-candidates-browser-boundary-clean-final-ctest.log`。生产 gate 仍为 `false`，未启动真实 JTDX、未连接 CAT/PTT/TX、未做 HIL/部署。详见 [`docs/web-ui/PROGRESS_zh-CN.md`](docs/web-ui/PROGRESS_zh-CN.md) 与 [`docs/web-ui/P4-FREQUENCY-FORM_验收_zh-CN.md`](docs/web-ui/P4-FREQUENCY-FORM_验收_zh-CN.md)。

### P4 频段筛选与常用频率候选小片

本片在 `main/e03e292` 上完成：候选来自 `Configuration::frequencies()` 的 `FrequencyList_v2` 与 `Bands`，按当前 mode/region 过滤、去重、排序并有界发布；前端频段筛选和常用频率选择只填入目标输入，不自动发送，手动输入保留。隔离浏览器夹具已验证 40m/20m 候选、选择后操作结果保持 `0` 条，以及 `390x844` 窄屏布局。此前 `17/20` 失败已定位为 DLL 搜索顺序混用运行库；按构建一致的 MSYS2 Qt 优先顺序重跑后，全量 CTest `20/20`、`100% tests passed`、`57.55 sec`，日志为 `C:\JTDX64\deps-webui\p4-frequency-candidates-final-ctest-6dbe33d.log`。生产 gate 仍为 `false`，未启动真实 JTDX、未连接 CAT/PTT/TX、未做 HIL/部署。详见 [`docs/web-ui/PROGRESS_zh-CN.md`](docs/web-ui/PROGRESS_zh-CN.md) 与 [`docs/web-ui/P4-FREQUENCY-FORM_验收_zh-CN.md`](docs/web-ui/P4-FREQUENCY-FORM_验收_zh-CN.md)。

### P4 频率表单浏览器故障注入与 HTTP 矛盾状态修复

基线为 `main/fb16e8b`，生产 `MainWindow` frequency gate 仍为 `false`。本轮使用真实 IAB 与隔离 `--serve-browser-frequency` 做人工故障注入；频率 POST 均被拦截，未发送给 fixture。超出 5 秒的传输请求显示结果未知并锁定发送；同 token 重新连接仍保持锁定；HTTP 200 的 `completed` 且 `confirmed=false`、`confirmed=true` 但回读频率错误、错误 `request_id`、非法 JSON 均保持未知锁。各场景顶部实际频率为 `14.074`，`operations` 为零。清除拦截后关闭页面，fixture 已停止。

另复现 HTTP 500 但 payload 为匹配 ID/epoch、`completed`、`confirmed=true` 且目标频率一致时，旧实现会在重新连接时错误解锁。`resources/web-ui/app.js` 已做最小修复：非 2xx 的矛盾 `completed` 不再清除请求身份，保留未知锁，等待匹配回读或新 epoch；合法的 `failed`/`rejected`/`timeout` 终态保持原行为。重建后的真实 IAB fixture `49153` 复测该场景后，同 token 重新连接仍为禁用，未知身份锁保留，顶部实际频率为 `14.074`。清除拦截后重载仅作另一用例隔离：`99999` 由服务端真实拒绝为 `invalid_frequency_hz` 且实际频率不变；随后 `14.075` 先 pending、再由匹配回读完成，顶部实际频率为 `14.075`，按钮重新可用。三目标构建退出码为 `0`，日志为 `C:\JTDX64\deps-webui\p4-frequency-fault-final-build.log`；全量 CTest `19/19` 通过、耗时 `58.74 sec`，日志为 `C:\JTDX64\deps-webui\p4-frequency-fault-final-ctest.log`。

上述是浏览器人工故障注入，不是自动 DOM 回归。页面 reload 仅用于用例隔离，不能据此声称未知锁可跨 reload 持久化；`server_epoch` 更换和真实设备仍未测试。本轮未启动真实 JTDX，未连接 CAT，未执行 PTT/TX/HIL，生产 gate 仍为 `false`。

### P4 手动频率表单与 capability fixture 验收

基于前端片段基线 `2e378fc`，本轮已完成原生手动频率表单、安全频率 POST、capability 状态门和结果回读；生产 `MainWindow` gate 仍为 `false`。隔离 `--serve-browser-frequency` 真实 TCP fixture 软件验收通过：`14.075000`、`14.076000` 两次独立请求均经历 pending 后由匹配回读完成，`99999` 由服务端以 `invalid_frequency_hz` 拒绝且实际频率保持上一成功值；`390` 视口 DOM `clientWidth=scrollWidth=375`，无水平溢出。构建退出码 `0`，日志 `C:\JTDX64\deps-webui\p4-frequency-form-final-build.log`；全量 CTest `19/19`、`58.86 sec`，日志 `C:\JTDX64\deps-webui\p4-frequency-form-final-ctest.log`。

本轮仅使用模拟 fixture，未启动真实 JTDX、未连接 CAT、未执行 PTT/TX/HIL；未声称完整视觉审查。异常传输、epoch/token 切换只完成静态实现，尚未专项浏览器验收；预设频率/频段、生产启用和高层隔离验证仍未完成。详见 [`docs/web-ui/P4-FREQUENCY-FORM_验收_zh-CN.md`](docs/web-ui/P4-FREQUENCY-FORM_验收_zh-CN.md)。

### P4 操作结果卡片隔离浏览器验收收尾

上一轮在五小时额度剩余 `29%` 时按规则停止新实现，但收尾额度耗尽，未写入结果。本轮恢复时额度为五小时 `100%`、周 `68%`；本轮不新增测试，仅补齐上一轮已完成的验收记录。隔离 `--serve-browser` 真实 TCP fixture 运行 5 分钟，使用只读数据、生产 `frequency gate=false`，模拟 4 条操作结果：`pending`、`timeout`、`failed`、`completed` 中文状态正确；仅 confirmed completed 显示 `14.075000 MHz`，其他值未知且顶部实际频率仍未知；陈旧快照提示正确。`1280x900` 与 `390x844` 视觉通过，窄屏 `clientWidth=scrollWidth=375` 无水平溢出；停止自建 fixture 后显示“连接断开，保留旧操作快照”，四条结果保留。权威构建日志 `C:\JTDX64\deps-webui\p4-operations-ui-final-build.log` 退出码为 `0`，全量 CTest 日志 `C:\JTDX64\deps-webui\p4-operations-ui-final-ctest.log` 为 `19/19`、`57.20 sec`。本轮未启动真实 JTDX、未连接 CAT、未执行 PTT/TX/HIL。

本次浏览器记录不覆盖 20 条上限、`server_epoch` 切换、异常字段浏览器场景或自动化 DOM 断言；这些仍是静态实现待专项。详见 [`docs/web-ui/P4-OPERATIONS-UI_验收_zh-CN.md`](docs/web-ui/P4-OPERATIONS-UI_验收_zh-CN.md)。

额度中止后续的 `request_id` 拒绝响应小片已完成有界回归：业务拒绝在可取得可信 ID 时保留规范化 `request_id`，无可信 ID 时生成新的规范化 UUID；生产频率 gate 仍为 `false`。受影响目标构建退出码为 `0`，定向 Web 回归 `3/3`、全量 CTest `19/19` 通过。独立证据分别见 `C:\JTDX64\deps-webui\p4-request-id-final-build-20260920-retry.log`、`C:\JTDX64\deps-webui\p4-request-id-focused-ctest-20260920-retry.log` 和 `C:\JTDX64\deps-webui\p4-request-id-final-ctest-20260920-retry.log`。此前失败日志 `p4-request-id-final-ctest-20260920.log` 的 6 项连锁失败来自 completion 夹具真实时钟脆弱性，不能作为最终结论。本次未启动 JTDX，未连接 CAT/PTT/TX，未做 HIL、浏览器或部署验证；详见 [`docs/web-ui/PROGRESS_zh-CN.md`](docs/web-ui/PROGRESS_zh-CN.md) 与 [`docs/web-ui/P4-FREQUENCY-HTTP_契约_zh-CN.md`](docs/web-ui/P4-FREQUENCY-HTTP_契约_zh-CN.md)。

## 当前状态

- 阶段：`P11 超时 Stop 恢复与交付边界核查`，P0/P1/P2/P3 已完成，P4 frequency/DX、P5 业务命令、P7 页面收口、P8 视口/断线、P9 DX/异常、P10 软件收尾和 P11 Stop 恢复/资源核查的隔离验证已完成；原生 MainWindow 人工操作、真实 CAT/DX/CQ 设备回读、HIL/部署未验证。
- 基线：本批从分支 `main`、HEAD `bd1a91c` 开始；结果提交号以本批本地提交为准。P4 HTTP 契约见 [`docs/web-ui/P4-FREQUENCY-HTTP_契约_zh-CN.md`](docs/web-ui/P4-FREQUENCY-HTTP_契约_zh-CN.md)，当前结果与恢复点见进度日志。
- 代码根目录：`C:\JTDX64\jtdx_sourcecode`。用户需求中的 `jtdx\_sourcecode` 按当前实际仓库路径解释。
- P3 历史范围：增加 Web UI 配置 Tab、MainWindow 唯一服务生命周期、菜单入口和 Qt Resource 原生深色响应式只读页；历史版本曾有摘要令牌，本批已移除其运行时和设置入口。服务仍为单进程主 Qt 事件循环，不新增 UDP/线程/进程或控制 API。
- 已确认：主程序已有 Qt5 Network、`MessageClient`、`MessageServer` 和 `JTDX_BUILD_LOCAL_TESTS`；P1 状态模型、P2 只读服务、P3 代码/资源/测试/文档、P4 生命周期/dispatch、HTTP 隔离基础、operations/SSE 有界回读、频率/DX/CQ/AutoSeq 显式配置门均已有本地修改；完整窗口和真实 CAT/DX/CQ 回读仍未完成。
- CAT 交付基线：代码提交 `328cc7a` 的错序检测及后续 artifact code HEAD `9984c38` 已由 `a89c9da` 记录最终 Release、完整 CTest `19/19`（含既有 `ftx1_cat_policy_test`）；该测试并非本 Web 文档批次新增。Web 仍只呈现实际状态，不把 CAT 构建证据或 HTTP `accepted` 当成设备回读或通联完成。
- 进程边界：Web 功能必须零新增进程、零新增常驻线程，优先使用 JTDX 主 Qt 事件循环。当前程序已有 `proc_jtdxjt9` 解码子进程，Web 任务不得把它误写成 Web 新增进程，也不得为了 Web 重构或删除它。

## 继续工作的最短路径

1. 先读本文件、`docs/web-ui/PROGRESS_zh-CN.md`，再读与当前阶段对应的设计文档。
2. 检查 `git status --short --branch`、`git log -1 --oneline`；若基线或工作树与记录不符，先更新恢复日志，不覆盖已有修改。
3. 每一阶段只做一个可审查批次：先源码事实和文件计划，再实现，再做该阶段静态/单元/API 验证，最后写恢复记录并提交中文 commit。
4. 按用户最新授权推进，不因额度百分比主动暂停，也不消费 reset；只有平台真实限制或工程边界阻塞时记录恢复点。每批仍需把已完成、未完成、命令和证据写入恢复日志，不把“请求已发出”当成完成。
5. P0 文档阶段不启动 JTDX、不连接真实电台、不执行 CAT/PTT/TX/HIL、不向群晖部署。用户后续继续到 P3/P6 时，可在隔离配置、`Rig=None` 且无真实 CAT 连接的条件下进行本地浏览器验证；开始前必须证明不会自动连接硬件。真实 CAT/PTT/TX 和无线电行为仍需单独授权。

## 阶段顺序

| 阶段 | 边界 | 交付重点 | 当前状态 |
| --- | --- | --- | --- |
| P0 | 文档与事实基线 | 需求、网络/进程安全边界、API 草案、验收矩阵、恢复入口 | 已完成 |
| P1 | 状态模型 | `JtdxWebState`、状态新鲜度、解码上限、事件循环安全读接口；只读数据接入 | 已通过（独立构建/14 项 CTest；未启动 JTDX/HIL） |
| P2 | 只读服务器 | `JtdxWebServer`、TCP 端口生命周期、`/`、`/healthz`、`/api/v1/state`、`/api/v1/decodes`、SSE | 已通过（loopback/API/CTest；未启动 JTDX/HIL） |
| P3 | 设置/菜单/前端骨架 | Web UI 设置 Tab、端口/绑定策略、菜单入口、内置资源、响应式只读页面 | 代码/资源/CTest/浏览器夹具复验已完成；完整 MainWindow 窗口人工验收留到 P6 |
| P4 | 普通控制 | `JtdxWebControl`、频率切换、过期解码 ID、DX 选择、状态回读 | 频率与 DX 软件安全门、独立显式配置、独立 generation/source 回读、跨客户端未确认锁和 MainWindow 隔离契约已通过；完整窗口/CAT 回读待后续批次 |
| P5 | 高风险控制 | CQ/AutoSeq 启动、停止流程、二次确认、幂等/超时/冲突和状态回读 | 软件命令与业务 generation 回读已通过隔离测试；真实设备和浏览器人工验收待后续 |
| P6 | 整体验证 | 自动化合同、浏览器手测、异常隔离、回归和交付报告 | 已由 P7-P11 分批完成有界软件/浏览器验收；完整 MainWindow 仍待后续 |
| P8 | 浏览器视口与生命周期 | 实际宽/窄视口、确认、断线重连、横向溢出 | 已完成有界浏览器验收；未覆盖真实 MainWindow/CAT |
| P9 | DX、在途停止与异常 | 新鲜/陈旧 DX、Stop 接管、取消/重复、timeout、epoch/身份边界 | 已完成隔离浏览器闭环；未覆盖真实设备/HIL |
| P10 | 软件交付收尾 | 错误身份回包、确认后重复提交、MainWindow 无硬件审查、报告与使用说明 | 已完成软件收尾；原生窗口人工验收、真实设备/HIL/部署未完成 |
| P11 | 超时 Stop 恢复与交付核查 | 主程序嵌入资源重建、未确认锁后的安全 Stop、结果字段和 MainWindow 集成边界 | 已完成最小软件修复、自动/浏览器证据与资源重建；原生窗口人工验收、真实设备/HIL/部署未完成 |
| HIL | 独立授权 | 真实 CAT/PTT/发射、设备反馈、长时间运行和无线电行为 | 未授权/未开始 |

P1 至 P11 每次只推进一个阶段；HIL 不属于普通阶段的默认验收。

下批最短路径：由根会话评估是否继续补齐完整 MainWindow 隔离验收、交付收口或另行授权 HIL；真实 CAT/PTT/TX/HIL 仍需单独授权。频率、DX、CQ/AutoSeq 的软件回读都不能替代设备证据。

本批最终证据：构建日志为 `C:\JTDX64\deps-webui\p4-frequency-http-final-build-2ca51ec.log`，其中确认重新编译 `jtdx_web_service_test` 的 `JtdxWebService.cpp`/`JtdxWebServer.cpp`；全量 CTest 日志为 `C:\JTDX64\deps-webui\p4-frequency-http-final-ctest-2ca51ec.log`，`19/19` 通过。此前 `LastTestsFailed` 或旧 service 二进制状态不作为本批结论。

## 事实锚点与构建入口

以下是 P0 读取源码所得事实，后续实现前仍需以当时源码复查：

- `main.cpp` 创建 `MainWindow`；`mainwindow.cpp` 持有两个 `MessageClient`，并连接现有状态、解码和控制信号。
- `MessageServer.cpp` 是既有 UDP 服务；Web 服务器只能新增独立 TCP 监听，禁止复用、改写或镜像为新的 UDP 监听器。
- `Configuration.hpp/.cpp` 已有 UDP 地址/端口和频率、收发器等配置入口；Web 配置应新增独立键名，不能改动现有 UDP 键。
- `mainwindow.cpp` 的 `band_changed()`、`setRig()`、`handle_transceiver_update()`、`statusUpdate()` 和解码路径，以及 `MessageClient::status_update()`，是后续状态/频率接入的事实核对点。当前不能把请求写入的目标频率当作 CAT 实际回读频率。
- 现有 CQ、AutoSeq、Enable Tx、停止流程有 UI 槽函数和内部副作用；后续必须先抽取或复用业务入口，不能从 Web 模拟鼠标点击或直接调用 PTT。
- `CMakeLists.txt` 生成 `jtdx`、`jtdxjt9` 等目标；Qt5 Network 已存在；`JTDX_BUILD_LOCAL_TESTS` 控制本地 CTest 目标。本阶段没有配置或构建新的目标。

已探测到的工具版本仅用于恢复参考：`C:\msys64\mingw64\bin\cmake.exe` 4.4.0、`g++.exe`/`gfortran.exe` 16.1.0 Rev5、`qmake-qt5.exe` Qt 5.15.19；未验证本项目配置/编译兼容性。后续先确认独立构建目录和依赖，不自动升级工具链。

建议的本地构建/测试形式（仅模板；只在实现阶段、确认依赖和独立构建目录后使用，不代表本轮执行结果）：

```powershell
C:\msys64\mingw64\bin\cmake.exe -G "MinGW Makefiles" -S C:\JTDX64\jtdx_sourcecode -B C:\JTDX64\build-webui-dev -DCMAKE_BUILD_TYPE=Release -DJTDX_BUILD_LOCAL_TESTS=ON -DWSJT_ENABLE_OMNIRIG=OFF -DCMAKE_C_COMPILER=C:\msys64\mingw64\bin\gcc.exe -DCMAKE_CXX_COMPILER=C:\msys64\mingw64\bin\g++.exe -DCMAKE_Fortran_COMPILER=C:\msys64\mingw64\bin\gfortran.exe
C:\msys64\mingw64\bin\cmake.exe --build C:\JTDX64\build-webui-dev --parallel 2
C:\msys64\mingw64\bin\ctest.exe --test-dir C:\JTDX64\build-webui-dev --output-on-failure
```

上述模板沿用当前 Hamlib-only 验证边界（`WSJT_ENABLE_OMNIRIG=OFF`）；现有历史构建目录和 `CMakeCache.txt` 不作为当前证据。若工具路径、Qt 或生成器不同，先记录探针结果再调整，不自动升级依赖。

## 文档索引

- [需求、边界与改进项](docs/web-ui/01_需求与边界_zh-CN.md)
- [设计、API 与安全契约](docs/web-ui/02_设计_API_安全_zh-CN.md)
- [阶段验收矩阵](docs/web-ui/03_阶段验收矩阵_zh-CN.md)
- [进度与中断恢复日志](docs/web-ui/PROGRESS_zh-CN.md)
- [P10 软件交付报告与简短使用说明](docs/web-ui/P10-软件交付报告_zh-CN.md)
# P23 最新交接

P23 当前本地候选、响应式布局/周期/DX 回读验证、提交、完整 CTest、ZIP 哈希及未验证边界见 `docs/web-ui/P23-电台状态紧凑布局与直接操作_zh-CN.md`。候选为本机审阅用途，不代表真实电台/HIL 或公开分发清权。

# P18 历史交接

QSO 草稿完整流程、截图差异、受限 radio API、1280/390 视口指标、浏览器操作回读和无 HIL 边界见 `docs/web-ui/P18-WebUI-QSO与视口验收_zh-CN.md`。P17 的历史实现与 RC 交接仍见 `docs/web-ui/P17-WebUI功能扩展与RC交接_zh-CN.md`。
