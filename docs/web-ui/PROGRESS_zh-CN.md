# JTDX 内置 Web UI：进度与中断恢复日志

# 2026-09-24 P26 Web 电台控制超时与六按钮收口（待根任务复核）

- P26 基线 `26cf365`，实现双重 prepare/begin 导致 Radio 原生处理器不运行的问题，并补齐嵌套安全读回字段；范围、映射、超时锁策略及验证边界见 [`P26-Web电台控制超时与六按钮收口_zh-CN.md`](P26-Web电台控制超时与六按钮收口_zh-CN.md)。
- 页面电台区收口为六项，保留 QSO 草稿取消/确认；解码卡全宽、内容可换行，DX 选择跨两行，窄屏不横向溢出。浏览器为本机 loopback 内存夹具，不是真实 MainWindow/ADIF/设备验证。
- 代码提交 `a1eaf35931233f849fb34bbefb5bb4274a15aa0d`；完整 Release 构建成功，全量 CTest 26/26、100%、58.92 秒。日志及 141 文件安装 manifest 位于 `C:\JTDX64\deps-webui\evidence\P26\`。
- 本机审阅 ZIP `C:\JTDX64\JTDX-2.2.159.2.10-local-a1eaf35-P26.zip` 与 SHA-256 sidecar 已生成；清洁解压逐文件比对待最终提交复建归档后完成。仅本机审阅，不表示根任务接受或公开发行；未做 CAT/PTT/TX/HIL、UDP 服务线程或部署验证。

# 2026-09-23 P25 解码地理字段与电台安全门纠偏（待根任务复核）

- P25 代码提交：`05ee60d5de8ae1d29b00df6d23badcf12cd435c6`，基线 `2d5ee7b0d75f439b4016a42b0ea69cbd0434c9f8`。更正 24 字符解码投影、DXCC entity/continent/中国省份映射、解码选择关联和动作级门控；停止动作可优先越过未知反馈锁，但不清锁。没有修改 UDP 协议或真实 CAT/PTT/TX 执行机制。
- 从干净 Release 构建目录完成完整应用及测试目标构建；最终成功日志 `C:\JTDX64\deps-webui\evidence\P25\final-build-retry.log`。首次并行构建因自动生成 UI 头文件时序失败，重跑全部目标成功；全量 CTest `25/25`、100%，最终日志 `C:\JTDX64\deps-webui\evidence\P25\final-ctest.log`。
- 本地 loopback HTTP/SSE 浏览器夹具在 1280/550/390 宽度分别确认文档无横向溢出；解码列可内部滚动，末端的选择操作可见。测试夹具截图与量测见 [`P25-解码地理字段与电台安全门纠偏_zh-CN.md`](P25-解码地理字段与电台安全门纠偏_zh-CN.md) 及 `C:\JTDX64\deps-webui\evidence\P25\`。
- 本机候选 ZIP `C:\JTDX64\JTDX-2.2.159.2.10-local-05ee60d-P25.zip`，SHA-256 `9D7A80E9EBDE4DC7A1C77E63A53E7DF0C5C30AA25F3E4E4ECB68CBE7E743C6FC`；141 个文件、根项 `bin/`、`plugins/`、`share/`、`NOTICE_zh-CN.md`；清洁解压后 141/141 文件 SHA-256 与 staging/manifest 一致。只供本机审阅，不是公开发行包。
- 持久打包规则：今后新生成的 Release ZIP 直接输出到 `C:\JTDX64` 根目录；不要在根目录散放历史包，历史候选保留在原归档位置并以明确的版本名区分。
- 未启动完整真实 MainWindow 做 GUI 验收，未连接/操作 CAT/PTT/TX、未发射、未做 HIL、LAN/公网验证或部署；第三方许可证及数据文件的法律/再分发义务未核实。阶段已向根任务报告待复核，不在此声称接受。

### 2026-09-23 P24 根审阅纠偏（历史；由 P25 后续覆盖）

- P23 尚未被根任务接受；本节只记录按根审阅意见完成的纠偏，不构成验收、发布或硬件验证结论。P23 的历史说明保留为历史记录，当前候选源码为 `6dbb210f4ef20ff2cc5ffbe1c2788bb9ea2f79c7`。
- 本批限定于 `resources/web-ui/{index.html,style.css,app.js}` 和 `tests/mainwindow_web_frequency_contract_test.cpp` 的布局、文案、输入保留/校验与周期校准，以及对应文档；没有修改 native 电台业务逻辑、CAT/PTT/TX、UDP 或服务安全门。
- 最终完整构建日志：`C:\JTDX64\deps-webui\evidence\P24-webui-fix-build.log`；最终全量 CTest：24/24，日志 `C:\JTDX64\deps-webui\evidence\P24-webui-fix-ctest-final.log`。浏览器 loopback 夹具的桌面/窄屏、固定顶部周期条、表单输入保留和错误反馈证据及限制见 [`P24-根审阅纠偏_zh-CN.md`](P24-根审阅纠偏_zh-CN.md)。
- 本地审阅包及 SHA-256、141 文件 manifest、干净解压逐文件比较见该恢复记录。包不是公开发行包；第三方法律清权、真实 MainWindow、CAT/PTT/TX、HIL、LAN/公网和部署仍未完成/未验证。等待根任务复核，不在此宣称 P23 验收。

### 2026-09-23 P23 电台状态紧凑布局与直接操作（历史）

- 当前行为与完整前后端映射见 [`P23-电台状态紧凑布局与直接操作_zh-CN.md`](P23-电台状态紧凑布局与直接操作_zh-CN.md)。该批覆盖历史章节中的旧页面和旧 Web 专属门控，历史记录保留但不代表当前行为。
- 当前批已实现：电台状态合并、桌面左右/窄屏纵向布局、JTDX 校时周期条、整数 Hz 频率目标与手动/解码 DX；移除 Web 专属确认/能力开关/快照与来源门控，保留原生安全和真实回读、loopback 默认监听。
- Release 全量构建已完成；全量 CTest、浏览器视口、安装包、最终提交及 SHA256 在验证完成后更新，不提前视作通过。

### 2026-09-21 P12 有界本地控制诊断日志（完成）

- 实现：复用 MainWindow 与 Hamlib 既有可写数据目录和 `jtdx_recovery.log`，所有同文件写入统一经过 `JtdxLocalLog`；不新增线程、服务、HTTP/远程日志或恢复状态源。主动文件上限 `256 KiB`，固定只保留一个 `.1` 轮转文件，沿用既有本地 ISO 毫秒时间格式和 UTF-8；超大既有文件先限长再轮转，超长单条也保持文件上限，轮转、打开或写入失败只丢弃诊断，不改变控制结果和安全门。
- 字段与事件：`reject`、`accepted`、`transition`、`duplicate` 事件记录本地 ISO 毫秒时间、规范化 `request_id`、操作、状态、限长原因、generation/state revision 和有限 readback 摘要。Web 字段值会清除换行/制表符、转义反斜杠和等号；通用写入器保留既有 `key=value` 分隔符以兼容 CAT/恢复解析；不写 Web Authorization、token、密码、原始 HTTP、call/grid 或凭据。重复同 ID 同 payload 只记录 `duplicate`，dispatcher 执行次数不增加。
- P11 核对：新 epoch 或多浏览器不会以重连、日志或日志文件内容解除 `unconfirmed_feedback`；服务级 latch 仍只允许当前安全 Stop，并要求当前 epoch/revision 及 `idle/disabled` 业务回读，完成 Stop 后仍保留旧操作未知锁。
- 验证：统一日志构建 `C:\JTDX64\deps-webui\p13-final-build-20260921.log`，页面资源构建 `C:\JTDX64\deps-webui\p13-ui-build-20260921.log`；`C:\JTDX64\deps-webui\p13-focused-20260921.log` 为定向 `6/6`、`100% tests passed`；最终全量 `C:\JTDX64\deps-webui\p13-final-ctest-dc62904-20260921.log` 为 `23/23`、`100% tests passed`、57.48 秒。新增测试覆盖临时目录写入、Unicode/换行注入、key=value 兼容、超长单条、超大既有文件、轮转失败、重复生命周期和写失败不影响控制。
- 边界：仅完成软件/loopback/自动证据；未启动真实 JTDX，未连接 CAT，未执行 PTT/TX/HIL、部署、LAN 或真实设备回读。
- 恢复口径：新 epoch、浏览器重连、停止/启动 Web 服务和日志操作都不清除服务级 `unconfirmed_feedback`；安全 Stop 只负责把业务回读到 `idle/disabled`。若普通控制仍锁定，需关闭并重启 JTDX 以重建 `JtdxWebControl`，不能自动重发或默认解锁。

### 2026-09-21 P7 Web 访问与页面布局收口（完成）

- 范围：移除 Web token 访问保护和前端令牌输入；保留默认 loopback、显式 LAN、Host/Origin/JSON/CSRF 边界、控制默认关闭、确认、幂等、超时、epoch/revision、业务 generation 与实际回读；同时精简页面开发文案并按 JTDX 结构重排桌面/移动布局。
- 实现：GET、SSE 和已开放控制 POST 不再要求或发送 Authorization；旧 `WebUiTokenSha256` 只安全忽略，不再显示、写回或进入 Service 签名。页面自动连接/断线重连；桌面左侧为运行/RX/解码/操作，右侧为频率/DX/TX/CQ/AutoSeq，窄屏改为左后右纵向排列且禁止横向溢出。
- 文案：移除 token 输入、开发合同、epoch/revision 等实现性展示，保留真实错误、未知/陈旧/断线提示和“登记/HTTP 成功不等于完成”的回读提示。
- 边界：使用无硬件 loopback fixture 和受支持浏览器观察页面；不处理既有浏览器残留页；未启动真实 `jtdx.exe`，未连接 CAT/PTT/TX，未做 HIL、部署或真实无线电验证。移动窄屏由静态契约覆盖，未声称真实 MainWindow 视觉验收。
- 验证：最终目标构建退出码 `0`，日志 `C:\JTDX64\deps-webui\p7-final-build-20260921.log`；全量 CTest `22/22`、`100% tests passed`、57.37 秒，日志 `C:\JTDX64\deps-webui\p7-final-ctest-20260921.log`；浏览器观察到桌面左右布局、自动连接、无令牌输入、陈旧提示和操作结果状态分层。

### 2026-09-21 P8 支持浏览器验收闭环（有界收尾）

- 基线：`main/1dea8b9`，开始时工作树干净；本批不改生产代码、不扩展测试 fixture。
- 实际宽屏：默认视口 `1280x720`；`workbench` 实测 `1064` 宽，左/右工作区各 `525`，`documentElement.scrollWidth=clientWidth=1265`，截图确认桌面双列。
- 实际窄屏：显式视口 `390x844`；`innerWidth=390`，`clientWidth=scrollWidth=375`；左工作区 `351` 宽、右工作区在其后纵向排列，截图确认无横向溢出。
- 页面业务：确认框为页面内 `role=dialog`，取消 CQ 后操作记录仍为空；确认 CQ、Stop、AutoSeq 均观察到登记中间态后由业务状态回读完成。模拟 CQ 状态显示活动 TX，Stop 可用；操作记录分别出现 `cq_armed`、`automation_stopped` 和 `auto_sequence_armed_waiting_for_decode`。
- 断线重连：停止自有 fixture 后观察到“未连接”、状态陈旧提示及旧操作记录保留；重启同一 fixture 后自动恢复“已连接/新鲜”。
- 收尾边界：检查时五小时窗口剩余约 `29%`，按规则停止新实现。既有 fixture 的 250ms 回读窗口不足以稳定完成“在途启动被 Stop 取代”专项时序；DX 有效/过期选择、浏览器级超时/身份不匹配和网络 POST 次数也未在本批补做。不能将本批隔离浏览器证据解释为完整 MainWindow、CAT/PTT/TX、HIL 或部署证据。

### 2026-09-21 P9 DX、在途停止与异常状态浏览器闭环

- 基线：`main/bd1a91c`，工作树开始时干净。本批只扩展 `tests/jtdx_web_server_test.cpp` 的 loopback 浏览器 fixture，不改生产代码、API、线程、UDP 或服务架构。
- Fixture：`--serve-browser-automation-p9` 固定端口 `49154`，提供新鲜解码、DX 选择匹配回读和 60 秒在途业务窗口；`--serve-browser-automation-p9-timeout` 固定端口 `49155`，使用测试时钟推进既有 `JtdxWebControl::expire()`，只用于浏览器 timeout 验收。
- DX：浏览器实际观察到唯一 `POST /api/v1/control/select-dx`，请求携带当前 `decode_id`；回读 `K1ABC/FN31`、原因 `feedback_matched`，TX 允许发射/发射中均为否。fixture stale 后 `freshness=陈旧`、DX 状态为“状态快照陈旧”、选择按钮数为 `0`，未产生过期选择 POST。
- 在途 Stop：AutoSeq 确认后页面先显示“命令已登记，等待业务状态回读”，Stop 在启动回读前确认；CDP 网络记录恰好 2 个 POST。最终 Stop 为 `completed/already_selected`，旧 AutoSeq 为 `rejected/superseded_by_stop`，未出现旧启动状态复活。
- 去重与取消：CQ 取消、重复点击启动按钮后取消，均观察到确认框只保留一个流程、POST 数为 `0`、操作记录无新增。已有 AutoSeq/Stop 组合只产生各自一次 POST。
- 跨 epoch：pending AutoSeq 后停止 fixture，页面显示未连接并保留处理中记录；重启同一 fixture 后显示“服务状态已刷新，旧 CQ/AutoSeq 请求已失效”，连接恢复但不误报完成，旧记录不跨新 epoch 复用。
- Timeout：timeout fixture 中 1 个 POST 先保持 pending，约 4.2 秒后显示 `超时/feedback_timeout`，按钮恢复可用，未显示业务完成。
- 自动验证：`p9-fixture-build-20260921-r9.log` 构建通过；`p9-server-focused-20260921.log` 为 `1/1`、34.34 秒；`p9-final-ctest-20260921.log` 为全量 `22/22`、57.51 秒。未启动真实 JTDX，未连接 CAT/PTT/TX，未做 HIL、部署或完整 MainWindow 验收。

### 2026-09-21 P10 软件交付收尾与 MainWindow 隔离审查

- 范围：只对 `resources/web-ui/app.js` 增加超时/未确认锁的前端保护，不改生产 API、线程、UDP、令牌或控制语义；并核对 `main.cpp` 的 `--test-mode`/`--rig-name` 实例锁、`Configuration` 默认 `Rig=None`/`AcceptUDPRequests=false`/Web 关闭、MainWindow 的延迟 `rigOpen()`、既有音频线程和 `jtdxjt9` 子进程边界。
- 浏览器身份专项：受支持浏览器工具没有直接 CDP 响应改写接口，因此使用临时本机 TCP 代理做测试流量故障注入。真实页面确认后，分别只改回包 `request_id`、只改回包 `server_epoch`；页面均保持“结果未知/处理中”保护，不绕过确认、不注入 fixture 内存。临时代理脚本已删除。
- 浏览器幂等专项：页面真实确认 `启用 AutoSeq` 后，代理把同一个已确认 POST `/api/v1/control/start-auto-call` 实际转发两次；页面最终只保留 1 条 `处理中/awaiting_feedback` 操作记录，代理日志 `C:\JTDX64\deps-webui\p10-proxy-duplicate.stdout.log` 记录 `DUPLICATE_FORWARD` 与 `DUPLICATE_COMPLETE`。这与“重复点击后取消、0 POST”是不同证据。
- MainWindow 安全启动：仅启动自有 `C:\JTDX64\build-webui-dev-msys2\jtdx.exe --test-mode --rig-name p10-mainwindow`，观察到自有 `jtdx.exe` 和既有 `jtdxjt9.exe`，随后均停止；未终止用户实例、未连接 CAT、未执行 PTT/TX。当前工具没有原生 MainWindow 可操作面，故不宣称设置 Tab、保存重载、菜单、端口冲突/重启/退出释放或完整窗口人工验收。
- 超时锁收口：最终浏览器观察到 `超时/feedback_timeout` 后启动按钮禁用、Stop 可用；确认 Stop 被服务端以 `unconfirmed_feedback` 拒绝后，启动按钮仍禁用。最终构建 `p10-timeout-ui-build-20260921-r2.log`、定向 `p10-timeout-ui-focused-20260921-r2.log`（3/3）和全量 `p10-timeout-ui-final-ctest-20260921-r2.log`（22/22）。
- 交付：新增 [`P10-软件交付报告_zh-CN.md`](P10-软件交付报告_zh-CN.md)，覆盖 14 项原交付要求、API/配置/调用路径、简短使用说明、浏览器证据、唯一权威构建/CTest 日志和未验证边界。P9 权威构建/测试日志不因本批纯文档与流量观察而重测。

### 2026-09-21 P11 超时 Stop 恢复、主程序资源重建与集成边界核查

- 构建来源：CMake 的 `add_resources(... /web-ui ... app.js)` 进入 `wsjtx_RESOURCES_RCC`，`jtdx` 目标实际包含该 RCC；`C:\JTDX64\deps-webui\p11-build-20260921-r3.log` 明确重新生成 `qrc_jtdx.cpp` 并链接 `jtdx.exe`，同时重建 Control/Server/Service 测试目标。测试 fixture 资源在 `p11-build-20260921-r5.log` 中重新链接；因此本批没有把测试 QRC 当作主程序资源证明。
- Stop 语义：源码原先在全局 `unconfirmed_latch_` 判断处连 Stop 一并拒绝，和已有 `safe_to_dispatch()` 高优先级 Stop 规则及 `MainWindow::applyWebStopAutoCall()` 的“先停 TX、再关闭 AutoSeq”路径不一致。最小修复仅允许 `StopAutoCall` 穿过该锁；启动、频率、DX 和其他改变操作仍拒绝 `unconfirmed_feedback`。Stop 仍须经过当前 epoch/revision、一次性 begin 和 idle/disabled 业务回读，完成后不清除旧 latch；迟到旧回调仍因 pending/request 校验失败而不能复活。
- 自动证据：`p11-pre-final-focused-20260921.log` 为配置、Control、MainWindow 合同、Server、Service `5/5`；最终 `p11-final-ctest-20260921.log` 为全量 `22/22`、58.38 秒。新增 Control 测试覆盖“已 begin 超时 → Stop 完成 → 启动仍被锁定”，Server fixture 让未确认 AutoSeq 进入 `armed/calling` 后再由 Stop 回读 `idle/disabled`。
- 浏览器证据：更新后的 timeout fixture 实际显示 `AutoSeq enabled` 与 `armed/calling`；超时后启动 CQ/AutoSeq 禁用、Stop 可用；确认 Stop 后操作结果为 `已完成（已确认）/automation_stopped`，页面回读 `idle/idle`、AutoSeq `disabled`，启动按钮仍禁用，状态显示“停止已由业务状态回读确认；服务端仍保留旧操作的未确认锁”。
- 前端收口：`resources/web-ui/app.js` 增加请求级 `preserveUnknown`，Stop 完成或 epoch 变化时不把旧未知操作误解锁；`node --check` 与 `git diff --check` 通过。当前页面不再把“Stop 可执行”与“旧不确定操作已被证明完成”混为一谈。
- MainWindow 集成评估：已有 `configuration_web_ui_test` 真实执行 Configuration 对话框取消/确认、持久化重载、默认 `Rig=None`/loopback/控制关闭和非法端口；仓库没有可复用的完整 MainWindow 测试构造/注入层，直接纳入会启动既有音频线程、解码子进程、QSettings 和 CAT 初始化，无法证明无硬件隔离，因此不做大规模测试重构。完整 Settings Tab、菜单重复、端口冲突/重启/退出释放和窗口视觉保留人工验收，并在 P11 报告附简短步骤/预期结果。
- 诊断与结果字段：未新增远程日志或日志 API；现有 `/api/v1/state`/SSE 有界 `operations` 实际保留 `request_id`、`received_ms`/`deadline_ms`/`completed_ms`、`reason`、generation 和 `readback.confirmed`，Server timeout/Stop 回归已观察这些字段。Web 不写新的本地诊断文件；现有 MainWindow recovery log 仍是既有音频/AutoSeq/CAT 诊断，不冒充 Web 访问日志。该边界作为明确未新增小项记录。
- 层级结论：软件实现/自动 CTest/loopback 浏览器已通过；原生 MainWindow 人工操作、真实 CAT/DX/CQ 状态回读、PTT/TX、HIL、部署和 LAN 启用仍未验证/未授权。

### 2026-09-21 P6 页面确认与浏览器业务回归收尾记录

- 范围：基于 `main/c6f14fd`，只处理页面确认可控性、CQ/AutoSeq/Stop 的 loopback 浏览器链路和交付记录；不启动真实 `jtdx.exe`，不连接 CAT/电台，不执行 PTT/TX/HIL，不改 UDP、不新增生产 API/线程/进程。
- 实现：用页面内 `role=dialog` 替换原生 `globalThis.confirm()`；默认焦点在取消，Escape/取消都在生成 request ID 和 POST 前结束，Tab 只在取消/确认之间循环；服务端 `confirm=true` 要求保持不变。操作卡补齐 `start-cq`、`start-auto-call`、`stop-auto-call` 中文名称。
- 夹具：CQ 回调模拟活动 TX/PTT；Stop 回调清除 TX/PTT 并回读 idle/disabled；若启动尚未完成即被 Stop 取代，旧回调不再写入业务状态。新增 `--serve-browser-automation-p6` 使用临时固定端口 `49153`，用于避免残留 `49152` 浏览器页，但本批未再进行 UI 操作。
- 浏览器人工证据：页面确认框显示且默认焦点为取消；取消后无操作记录；确认后先 pending，再由匹配 business generation/readback 完成，`cq_armed` 且操作卡仅 1 条。原生确认框遗留页无法由 Computer Use 安全层清理，故后续 `slow active count=2` 失败保留为环境污染证据，不归因源码。
- 自动证据：构建 `C:\JTDX64\deps-webui\p6-dialog-fixture-build-20260921.log`；页面/主窗口静态与 `mainwindow_web_frequency_contract_test` 通过日志 `C:\JTDX64\deps-webui\p6-dialog-test-fix-focused-20260921.log`；排除受污染 `jtdx_web_server_test` 后其余 CTest `21/21`、`100% tests passed`，日志 `C:\JTDX64\deps-webui\p6-dialog-final-no-server-ctest-20260921.log`。`p6-dialog-fixture-focused-20260921.log` 中 server 失败为残留浏览器 SSE 污染；此前同批无残留时 server 单项通过，但不能替代本批清理后的全量证据。
- 未验：AutoSeq/Stop 的完整浏览器人工确认链、重复点击、断线重连、epoch/身份不匹配和 timeout 的浏览器场景；完整 MainWindow 窗口、真实 CAT/DX/CQ 回读、PTT/TX/HIL、部署仍未验。AutoSeq 仍为 decode-driven 等待实时解码，不表示立即呼叫当前 DX。

### 2026-09-21 P5 启停语义审查、修复与隔离回归

- 基线：`main/bfa0c34`，工作树开始时干净；本批只处理 CQ/AutoSeq 请求到 MainWindow 业务入口的语义，不启动真实 `jtdx.exe`，不连接 CAT/PTT/TX，不改 UDP、不新增 Web 线程/进程。
- 源码复核确认并修正：`start-cq` 复用 `on_txb6_clicked()` 并在 Web 安全门之后进入既有 `enableTx_mode(true)`；`start-auto-call` 只启用既有 decode-driven AutoSeq，并显式提示等待下一批实时解码；`stop-auto-call` 先调用既有停止入口、再关闭 `m_autoseq`，且可优先处理活动 TX/PTT 状态。停止也能安全取代尚未完成的启动请求。
- 前端：启动/停止分别使用操作安全门；停止在活动 TX/PTT 时仍可用；AutoSeq 文案不再暗示已经生成具体呼叫或发射。顶部能力提示按 frequency/DX/CQ/AutoSeq 实际开关显示。
- 自动验证：`jtdx_web_control_test`、`mainwindow_web_frequency_contract_test` 定向为 `2/2`；`jtdx_web_server_test`（含 loopback API 的活动停止和浏览器夹具代码）为 `1/1`；最终清理浏览器测试页后全量 CTest 为 `22/22`、`100% tests passed`、57.76 秒。构建日志为 `C:\JTDX64\deps-webui\p5-semantic-final-build-20260921.log`，定向构建/测试日志为 `C:\JTDX64\deps-webui\p5-semantic-gate-build-20260921-r3.log`、`C:\JTDX64\deps-webui\p5-semantic-browser-build-20260921.log`、`C:\JTDX64\deps-webui\p5-semantic-gate-focused-20260921-r3.log`、`C:\JTDX64\deps-webui\p5-semantic-browser-focused-20260921.log`，最终全量日志为 `C:\JTDX64\deps-webui\p5-semantic-final-ctest-clean-20260921.log`。
- 浏览器隔离夹具已确认连接后 CQ/AutoSeq 控件可用并出现启动二次确认文本；原生确认框阻塞了本次自动化会话，未把取消后“无 POST”写成已验证事实，需后续可控会话补验。未做完整 MainWindow 窗口、真实 CAT/DX/CQ 回读、PTT/TX/HIL、部署或生产启用验证。

### 2026-09-21 P5 CQ/AutoSeq 启停软件链路批次

- 新增独立 `WebUiAutomationControlEnabled` 配置，默认关闭；新增 `start-cq`、`start-auto-call`、`stop-auto-call` 三个 POST 命令，命令体必须带 `confirm: true`，前端每次提交前再做二次确认。
- `JtdxWebControl` 增加业务命令 dispatcher 和独立 `business_generation` 回读；`JtdxWebState` 只有业务字段实际变化时才递增该 generation。HTTP `accepted/pending` 不代表完成，完成必须匹配 `cq_state` 或 AutoSeq 开关的业务回读。
- MainWindow 通过窄的业务适配方法执行 CQ/AutoSeq 状态转换并在主 Qt 事件循环中回读；未新增线程、进程、UDP、PTT 或 CAT 入口。停止命令复用既有停止优先级，页面不会无确认发送。
- 软件验证覆盖 Control 业务 generation、Server 三类命令中的确认/排队/回读、配置持久化、Service 签名、MainWindow 静态契约、前端 `node --check`；完整 CTest 待本片收尾后记录。
- 未验证：未启动真实 `jtdx.exe`，未连接真实 CAT/PTT/TX，未做真实发射、HIL、完整窗口人工验收、浏览器人工验收或部署；软件业务回读不能证明无线电已经发射。

### 2026-09-21 P4 DX 选择软件链路批次

- 基线：`main/ced90ea`；继续保持默认关闭、loopback/令牌/同源与主 Qt 事件循环边界。新增独立 `WebUiDxControlEnabled` 配置，不与频率控制开关隐式耦合。
- 已实现：`JtdxWebDx` 纯校验覆盖规范化呼号、协议字段、复合呼号和 4/6/8/10 位 Grid；`JtdxWebState` 按有界新鲜实时 `decode_id` 提供选择快照，并维护独立 `dx_generation`、来源和 DF/时间元数据。
- 已实现：`POST /api/v1/control/select-dx` 只接受 `request_id/server_epoch/state_revision/decode_id`，服务端重新检查解码新鲜度与纯校验；MainWindow 只更新 DX 输入投影并产生 DX 业务观测，不调用桌面双击、QSO 历史、标准消息、AutoSeq、Enable TX、PTT 或 TX 路径。完成只接受独立 DX generation 与目标/来源回读匹配。
- 前端：新鲜实时解码行提供“选择 DX”按钮；请求身份、epoch、超时和未知结果沿用频率控制的锁定语义，只有 `confirmed` 且 call/grid/source/decode_id 全匹配才显示完成。
- 自动验证：纯校验、状态选择/代际、Control、Server HTTP/SSE、Service、配置持久化、MainWindow 静态副作用契约与 `node --check`；日志待本片最终回归后补写。
- 未验证：未启动真实 `jtdx.exe`，未连接真实 CAT/PTT/TX，未做 HIL、完整 MainWindow 人工窗口、浏览器人工验收或部署；本批 DX 软件回读不能证明真实电台动作。

### 2026-09-21 P4 频率恢复边界与显式启用批次

- 基线：`main/961706e`，开始时工作树干净；五小时额度开始约 `87%`，本批按低于 `30%` 停止新实现。范围限定为频率恢复边界、高层 MainWindow 适配隔离契约、配置/菜单流程和跨客户端未确认锁；不进入 DX/CQ/AutoSeq。
- 已完成：`Configuration` Web UI 频率控制复选框恢复为可交互；专用 `WebUiFrequencyControlEnabled` 默认关闭并持久化。`MainWindow::applyWebUiConfiguration()` 将用户显式开关传给 Web 服务；`JtdxWebControl` 仍在执行前重读安全状态和 CAT 基线，`dispatchWebFrequency()` 继续复用 `band_changed()`，完成只接受真实 rig generation/频率回读。
- 新增 `tests/mainwindow_web_frequency_contract_test.cpp`：静态锁定 MainWindow 观察字段、`prepare_dispatch → begin_dispatch → band_changed` 顺序、无 TX/PTT 入口，并用 Qt-only 隔离 Control 模拟 Rig=None 拒绝与后续匹配回读完成。该测试是高层适配契约，不是完整 MainWindow 窗口或真实设备测试。
- `tests/jtdx_web_server_test.cpp` 新增跨客户端场景：一个 HTTP/SSE 客户端超时后，第二个 request_id/客户端仍收到 `409 unconfirmed_feedback`，不能重放；既有 Service 测试继续覆盖 stop/restart 新 epoch、旧 queued/begun dispatch 和同一 Control 重绑定。
- 构建：`cmake -S C:\JTDX64\jtdx_sourcecode -B C:\JTDX64\build-webui-dev-msys2` 后，目标 `jtdx configuration_web_ui_test jtdx_web_control_test jtdx_web_server_test jtdx_web_service_test mainwindow_web_frequency_contract_test` 构建退出码 `0`；日志 `C:\JTDX64\deps-webui\p4-frequency-gate-build.log`。
- 定向回归：`mainwindow_web_frequency_contract_test configuration_web_ui_test jtdx_web_control_test jtdx_web_server_test jtdx_web_service_test` 为 `5/5`，日志 `C:\JTDX64\deps-webui\p4-frequency-gate-focused-ctest.log`。
- 完整回归：CTest `21/21`、`100% tests passed`、57.25 秒，权威日志 `C:\JTDX64\deps-webui\p4-frequency-gate-final-ctest.log`。构建警告为既有弃用/截断警告，没有新增失败。
- 未运行：真实 `jtdx.exe`、真实 CAT/PTT/TX、HIL、部署和完整 MainWindow 人工窗口验收。下一批进入 DX 选择；不能把本批软件安全门当成真实 CAT 完成证明。

### 2026-09-21 P4 频率候选边界浏览器验收

- 在既有 loopback `--serve-browser-frequency` 夹具上增加两个测试专用场景：`--serve-browser-frequency-empty` 发布 `FT4 / Region 3` 空候选；`--serve-browser-frequency-invalidated` 先发布 FT8/All 候选，随后通过既有 SSE 快照切换到 FT8/Region 2 并移除已选的 `7.074000`。没有新增生产端点、后台服务或控制能力。
- 人工浏览器验收分别确认：空候选时频段/预设下拉只保留占位项，手动 `14.075000` 可输入且发送操作仍为 `0` 条；候选失效后上下文显示 `FT8 / Region 2 · 2 条候选`，预设选择清空但目标输入仍保留 `7.074000`；选择候选本身不产生 POST/操作结果；停止夹具后显示未连接、连接断开、保留旧操作快照，发送按钮禁用。
- 自动化边界仍由既有 `jtdx_web_state_test` 的空列表/上下文/revision 断言和 `jtdx_web_frequency_candidates_test` 的过滤/排序/上限断言覆盖；本轮浏览器结果是人工观察，不冒充自动 DOM 回归。
- 测试夹具目标重建退出码为 `0`；清理浏览器页及夹具重连后，受影响 `jtdx_web_server_test` 为 `1/1` 通过（36.39 秒）。最终完整 CTest 为 `20/20`、`100% tests passed`、57.18 秒；日志：`C:\JTDX64\deps-webui\p4-frequency-candidates-browser-boundary-clean-final-ctest.log`。此前孤立失败由仍在自动重连的临时浏览器页污染慢 SSE 夹具，未修改断言、超时或背压上限。
- 生产 `MainWindow` frequency gate 仍为 `false`；未启动真实 `jtdx.exe`，未连接 CAT/PTT/TX，未做 HIL 或部署。下一片如继续，应优先补自动化浏览器/页面合同能力评估，不扩展新的控制功能。

### 2026-09-20 P4 频段筛选与常用频率候选小片

- 基线为 `main/e03e292`，本片只推进频率表单候选展示，不改变生产写控制边界。候选由 `Configuration::frequencies()` 的 `FrequencyList_v2` 原始项构建，按当前 mode/region 与 `Bands::find()` 过滤、去重、按 Hz 排序；生产候选默认上限 200，状态快照硬上限 500。未读取 `QComboBox`，未改既有 model filter，JS 不保存频率表。
- `JtdxWebState` 新增有界 `frequency_candidates` 快照及 mode/region 上下文；`MainWindow::statusUpdate()` 复用现有状态刷新点发布候选。Web 页面新增频段筛选和常用频率选择；选择只填入原有 MHz 输入，不触发 POST、不触发 TX，手动输入仍可用；候选失效时只清空预设选择，不覆盖手动文本。
- 新增 `JtdxWebFrequencyCandidates` 单元测试覆盖 All/Region、FT8/JT9、Bands 过滤、重复频率默认标记合并、排序、上限和空结果；状态测试覆盖快照字段、去重和 revision 稳定性。受影响目标构建退出码为 `0`，`node --check resources/web-ui/app.js` 退出码为 `0`；新增/相关定向 CTest 曾为 `3/3` 通过。
- 隔离 `--serve-browser-frequency` 本地夹具验收：连接后浏览器显示 `FT8 / All · 4 条候选`，频段筛选 `40m/20m` 正常；选择 `7.074000 MHz` 后目标输入变为 `7.074000`，操作卡仍为 `0` 条；随后手动输入 `14.075000` 仍可用。`390x844` 窄屏截图确认频率表单纵向排列、无横向溢出。夹具已停止。
- 早先一次 `17/20` 结果不是源码回归：复现确认测试命令把 `C:\JTDX64\159\bin` 放在 Qt DLL 前，GUI/service 测试混入两套 Qt/运行库而以 `0xc0000602` fail-fast 退出；慢 SSE 失败也来自同一轮混用环境。改为构建一致的 `PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;C:\JTDX64\159\bin;...` 后，`configuration_web_ui_test`、`jtdx_web_server_test`、`jtdx_web_service_test` 均通过。
- 权威全量 CTest 为 `20/20`、`100% tests passed`、`57.55 sec`；日志：`C:\JTDX64\deps-webui\p4-frequency-candidates-final-ctest-6dbe33d.log`。生产 `MainWindow` frequency gate 仍为 `false`。
- 未启动真实 `jtdx.exe`，未连接 CAT/PTT/TX，未做 HIL 或部署；本片只使用本地隔离浏览器夹具和自动化测试。后续如复跑必须保留上述 DLL 顺序，避免再次混用运行库；候选空列表的浏览器切换场景仍可作为下一片专项，生产频率写入口继续关闭。

### 2026-09-20 P4 频率表单浏览器故障注入与身份锁修复

- 最终收尾检查点：五小时额度剩余 `28%`（已用 `72%`），已低于用户规定的 `30%` 阈值；本轮停止新实现，仅保留本地结果提交和交接。
- 基线为 `main/fb16e8b`；开始本轮时五小时额度为 `100%`，生产 `MainWindow` frequency gate 保持 `false`。本轮范围仅为 `resources/web-ui/app.js` 与本节指定中文文档；未启动真实 JTDX，未连接 CAT/PTT/TX，未做 HIL。
- 真实 IAB + 隔离 `--serve-browser-frequency` 人工故障注入中，频率 POST 被拦截，未发送给 fixture。验证了：传输超过 5 秒后结果未知且锁发送；同 token 重新连接仍锁；HTTP 200 `completed`/`confirmed=false`、HTTP 200 `completed`/`confirmed=true` 但 wrong Hz、错误 `request_id`、非法 JSON 均锁发送。上述场景顶部实际频率为 `14.074`，`operations` 为零；清除拦截后关闭页面，fixture 已停止。
- 额外复现 HTTP 500 + 匹配 ID/epoch + `completed`/`confirmed=true`/目标频率的矛盾响应：旧实现会在点击重连后错误解锁。前端最小修复让结算逻辑接收 `response.ok`，非 2xx 的矛盾 `completed` 保留 `frequencyRequest` 与 `frequencyUnknown`，合法 `failed`/`rejected`/`timeout` 终态维持原行为。重建后的真实 IAB fixture `49153` 复测后，同 token 连接仍为禁用，未知身份锁保留，顶部实际频率为 `14.074`。
- 清除拦截后重载仅作另一用例隔离：`99999` 由服务端真实拒绝为 `invalid_frequency_hz` 且实际频率未改；随后 `14.075` 先 pending、再由匹配回读完成，顶部实际频率为 `14.075`，按钮重新可用。
- 三目标构建退出码为 `0`，日志：`C:\JTDX64\deps-webui\p4-frequency-fault-final-build.log`。全量 CTest `19/19` 通过、耗时 `58.74 sec`，日志：`C:\JTDX64\deps-webui\p4-frequency-fault-final-ctest.log`。
- 以上为浏览器人工故障注入，不是自动 DOM 回归。页面 reload 仅作为用例隔离，未验证未知锁跨 reload 持久化；`server_epoch` 更换与真实设备仍未测。生产 gate 仍为 `false`。
- 下批最短路径：先专项覆盖 `server_epoch` 更换、未知结果跨页面与服务端 `unconfirmed_latch` 边界；随后让频段/预设候选来自 `Configuration::frequencies()`、按 region/mode 过滤的 `FrequencyList_v2` 迭代器与 `Bands`，不在 JS 硬编码、不读取 `QComboBox`、不改既有 model filter。生产 frequency gate 继续保持 `false`，直至高层隔离验证完成。

### 2026-09-20 P4 手动频率表单隔离验收收尾与提交恢复

- 基线为 `main/2e378fc`。上一轮已完成本批源码、测试、三份前端资源和中文文档，但额度耗尽未提交；本轮仅恢复收尾记录并精确提交，不改代码、不重测。
- 权威构建日志 `C:\JTDX64\deps-webui\p4-frequency-form-final-build.log` 退出码为 `0`；全量 CTest 日志 `C:\JTDX64\deps-webui\p4-frequency-form-final-ctest.log` 为 `19/19` 通过、总耗时 `58.86 sec`。日志尾部确认主程序、Web Server/Service 和测试目标完成链接，CTest 全部通过。
- 隔离 `--serve-browser-frequency` 真实 TCP fixture 验收：`14.075000` 首次请求先 pending 后由匹配回读完成并更新顶部实际频率；`14.076000` 以独立 request ID 成功；`99999` 被服务端以 `invalid_frequency_hz` 拒绝且实际频率保持不变。`390` 视口 DOM `clientWidth=scrollWidth=375`，无水平溢出；不宣称全面视觉审查。
- `productionMainWindow` gate 保持 `false`；本次仅使用测试可执行文件和隔离 fixture 验收，未增加线程、UDP 监听或生产进程。未启动真实 `jtdx.exe`，未连接 CAT，未执行 PTT/TX/HIL。
- 当前剩余门：预设频率/频段选择、异常客户端专项（异常传输、epoch/token 等场景）和生产启用仍未完成。提交前只执行 `git diff --check`，不 push、amend 或 reset；额外用户改动不纳入本批。

### 2026-09-20 P4 操作结果卡片隔离浏览器验收收尾

- 上一轮在五小时额度剩余 `29%` 时触发收尾阈值并暂停新实现，但收尾额度耗尽，验收结果未写入；本轮恢复额度为五小时 `100%`、周 `68%`。本轮不新增测试，仅补齐上一轮已完成的结果记录。
- 隔离 `--serve-browser` 真实 TCP fixture 运行 5 分钟，使用只读数据，生产 `frequency gate=false`，模拟 4 条操作结果。浏览器已观察到 `pending`、`timeout`、`failed`、`completed` 中文状态正确；仅 confirmed completed 显示 `14.075000 MHz`，其他频率值未知，顶部实际频率仍未知；陈旧快照提示正确。
- `1280x900` 宽屏与 `390x844` 窄屏视觉通过；窄屏 `clientWidth=scrollWidth=375`，无水平溢出。停止自建 fixture 后显示“连接断开，保留旧操作快照”，四条结果仍保留。
- 权威构建日志：`C:\JTDX64\deps-webui\p4-operations-ui-final-build.log`，构建退出码 `0`；权威全量 CTest 日志：`C:\JTDX64\deps-webui\p4-operations-ui-final-ctest.log`，`19/19` 通过，总耗时 `57.20 sec`。
- 未启动真实 `jtdx.exe`，未连接 CAT，未执行 PTT/TX/HIL。本次浏览器验收不覆盖 20 条显示上限、`server_epoch` 切换、异常字段浏览器场景或自动化 DOM 断言；这些仍是静态实现待专项。
- 结果提交前保持生产 gate 为 `false`；本轮仅更新验收文档、入口和进度日志并精确提交，不 push、amend 或 reset。

### 2026-09-20 P4 频率 POST 拒绝响应 request_id 修复批次开始检查点

- 开始额度：五小时剩余 `44%`、周剩余 `91%`；重启后的工作权限已恢复。本批五小时剩余低于 `30%` 时停止实现，仅做有界验证、文档、精确提交和交接。
- 基线：仓库 `C:\JTDX64\jtdx_sourcecode`，分支 `main`，HEAD `758fad2`，工作树干净。
- 目标：公开 `JtdxWebControl::normalize_request_id` 纯函数供 WebServer 复用，使频率 POST 业务拒绝在完整 JSON 可取得可信 ID 时返回原样规范化 ID；无可信 ID 仍由现有错误响应生成 UUID。
- 范围：仅涉及频率 POST 业务拒绝响应和真实 TCP 最小测试；不改 401/Origin/传输层 text，不启用生产 gate，不碰前端、DX/CQ、UDP 或 CAT，不运行真实 JTDX/HIL。

### 2026-09-20 P4 频率 POST 拒绝响应 request_id 修复批次收尾

- 已完成：`JtdxWebControl::normalize_request_id` 对 WebServer 公开；有效 JSON 对象中可规范化的 `request_id` 会在业务拒绝 JSON 中保留去首尾空白后的可信 ID；缺失、非字符串或不可打印 ASCII 的 ID 继续生成新的规范化 UUID，字段类型错误返回 `invalid_control_fields`，字符串不可规范化返回 `invalid_request_id`。401、Origin 和传输层拒绝仍保持既有 text 响应。
- 先前失败证据：`C:\JTDX64\deps-webui\p4-request-id-final-ctest-20260920.log` 中旧 `jtdx_web_server_test` 为 `6` 项失败，根因是 completion 夹具 `Control{1000}` 使用真实时间，导致 pending/completed/SSE 断言连锁失败；不是生产 CAT 失败。额度中止前已加入首个 Control 的 `set_clock_for_test(0)`，本次仅做回归收尾。
- 最终构建：在 `C:\JTDX64\build-webui-dev-msys2` 以 `C:\msys64\mingw64\bin` 优先、`C:\JTDX64\159\bin` 补 Hamlib，目标 `jtdx`、`jtdx_web_control_test`、`jtdx_web_server_test`、`jtdx_web_service_test`，退出码 `0`；日志：`C:\JTDX64\deps-webui\p4-request-id-final-build-20260920-retry.log`。
- 定向回归：`jtdx_web_control_test`、`jtdx_web_server_test`、`jtdx_web_service_test` 为 `3/3` 通过；日志：`C:\JTDX64\deps-webui\p4-request-id-focused-ctest-20260920-retry.log`。
- 全量回归：CTest `19/19`、`100% tests passed`，总耗时 `58.39s`；日志：`C:\JTDX64\deps-webui\p4-request-id-final-ctest-20260920-retry.log`。本次未启动 `jtdx.exe`，未连接 CAT/PTT/TX，未做 HIL、浏览器或部署验证；生产 `frequency gate` 仍为 `false`。
- 当前状态：源码、测试和三份中文文档待精确本地中文 commit；不 push、不 amend、不 reset。此前因额度中止而未完成的回归现已由上述独立日志补齐。

### 额度检查点（2026-09-14 18:50，北京时间）

五小时剩余 `88%`、周剩余 `1%`；因周额度不足暂停实现，未触发五小时剩余 `30%` 阈值。基线：`main/19453a8`；生产 `frequency gate` 仍为 `false`。下一片：固定有效 `request_id` 拒绝响应一致性及错误 JSON 边界，之后进行前端与隔离浏览器验收。本次未编译、未重测、未做 HIL、未启动 JTDX、未部署。

### P4 operations 状态/SSE 有界回读小片开始检查点（2026-09-14）

- 任务编号：`JTDX-WEBUI-P4-OPERATIONS-20260914`；基线：`main/d67fb66`，开始时工作树干净；开局五小时剩余约 `90%`、周剩余约 `13%`，中途收尾快照为五小时 `33%`、周 `4%`，最终暂停快照为五小时 `28%`、周 `3%`；本片五小时额度收尾阈值按用户要求为低于 `30%`（入口旧记录中的 `20%` 已纠正），周预算继续受限时只做有界收尾和交接。
- 目标：复用现有 `JtdxWebControl` 的最多 128 条结果记录，提供事件循环安全的有界 operations 摘要；将其加入既有 state GET 回读，并在 operation 状态变化时通过既有 SSE snapshot（独立于 State revision 的 event id）推送。结果必须保留 `request_id`、状态、原因、时间字段和有限 readback；不递归嵌入 `current_state`，不暴露令牌或凭据。
- 文件计划：`JtdxWebControl.hpp/.cpp` 增加稳定有序结果摘要与 operation revision；`JtdxWebServer.hpp/.cpp` 增加 operations 投影、包含 control generation 的 SSE event id 与 publish 触发；`tests/jtdx_web_server_test.cpp`、`tests/jtdx_web_control_test.cpp` 覆盖 pending/completed/timeout、重复 ID、state GET/SSE、重连和 epoch/停止清理；同步本入口与 P4 frequency 契约。
- 安全/范围：生产 `enable_frequency_control` 保持 `false`；不扩 DX/CQ/前端，不改 UDP/解码/CAT，不新增线程/进程，不运行 JTDX、CAT、PTT、TX 或 HIL；不重新实现 Control。
- 验证计划：在 `C:\JTDX64\build-webui-dev-msys2` 以 `C:\msys64\mingw64\bin` 优先、`C:\JTDX64\159\bin` 补 Hamlib、`QT_QPA_PLATFORM=offscreen` 构建所有受影响目标并跑完整 CTest；真实 TCP 仅使用隔离生产 Control 夹具，证据写入 `C:\JTDX64\deps-webui\p4-results-final-*.log`。

### P4 operations 状态/SSE 有界回读小片结果（2026-09-14）

- 已实现：`JtdxWebControl` 返回最多 128 条稳定排序的结果副本，并以独立 operation revision 表示新增、状态完成/超时、epoch 清理和迟到反馈原因变化；不暴露内部 `Record&`，不新增线程或信号重入路径。
- 已实现：既有 `/api/v1/state` 快照加入 `operation_revision` 与按当前 server epoch 过滤的 `operations`；每项含 `request_id`、operation/status/reason、接收/截止/完成时间、generation 和有限 `readback`。`readback.confirmed=false` 明确 timeout 保留值不是当前实际读回；不嵌套 `current_state`，不包含令牌。
- 已实现：SSE 继续只发送既有 `snapshot` 事件；event id 使用当前服务 epoch、State revision、operation revision 和 Server 本地 Control 代次的摘要，Control 状态变化即使 State revision 不变也会触发 snapshot。Control 替换/销毁和新 epoch 均能产生新 id；旧 epoch 结果不进入新快照，Last-Event-ID 仍走有界 resync + 全量 snapshot。
- 验证：定向构建 `jtdx configuration_web_ui_test jtdx_web_control_test jtdx_web_server_test jtdx_web_service_test jtdx_web_state_test jtdx_web_frequency_test` 退出码 0，日志 `C:\JTDX64\deps-webui\p4-results-final-build-20260914_091331.log`；完整 CTest `100% tests passed out of 19`、总耗时 `59.06s`，日志 `C:\JTDX64\deps-webui\p4-results-final-ctest-20260914_091402.log`。
- 未验证/未交付：未运行 `jtdx.exe`，未连接 CAT/PTT/TX/电台，未做浏览器/HIL/部署；MainWindow 生产 `enable_frequency_control=false` 保持不变，未扩 DX/CQ/前端、UDP、解码或 CAT。
- 后续门：根代理只读审查确认，解析有效 `request_id` 的 `invalid_frequency`/`invalid_state_revision` 等拒绝目前仍返回新生成的 request id，需在合同中固定一致行为；401、Origin 和传输层拒绝仍为 text 响应，需明确安全 JSON 边界且不能泄露 state；`resources/web-ui` 当前没有 operations 展示或 frequency 表单，需完成隔离浏览器验收后再评估是否进入 frequency CAT 生产适配。本片不解除生产 gate。
- 证据边界：Control 销毁时推进 Server 本地代次属于实现与静态审查结论；本片新增测试未单独断言 Control 销毁分支，不能将该分支描述为独立测试覆盖。
- 结果基线：本片中文档与源码结果提交为 `3f3a799`；工作树在提交后干净。

### P4 频率 HTTP 最小切片开始检查点（2026-09-14）

- 任务编号：`JTDX-WEBUI-P4-FREQUENCY-HTTP-20260914`；基线：`main/7c79be7`，开始时工作树干净；开局五小时剩余约 `98%`、周剩余约 `28%`，中途现场快照为 `83%`/`26%`。用户授权本片持续至五小时剩余低于 `30%`，之后只做构建、CTest、中文文档、精确提交和交接。
- 目标：真实 TCP `POST /api/v1/control/frequency` 进入现有 `JtdxWebControl::submit` 和生产 frequency dispatcher 的隔离夹具；仅允许显式专用频率控制开关，默认关闭。保持只读页面、UDP、CAT 恢复和 159 运行目录边界。
- 契约与文件计划：详见 [`P4-FREQUENCY-HTTP_契约_zh-CN.md`](P4-FREQUENCY-HTTP_契约_zh-CN.md)；源码范围为 `JtdxWebServer.*`、`JtdxWebService.*`、`Configuration.*`/UI、`mainwindow.cpp` 的配置投影，以及 `tests/jtdx_web_server_test.cpp` 的真实 TCP 隔离夹具。请求体 4 KiB、头 16 KiB、5 秒头超时，拒绝 TE/Expect/重复长度/管线/畸形 UTF-8/JSON。
- 当前状态：HTTP body 解析、频率路由、Control 接线与专用配置 gate 已实现；隔离测试和最终全量 CTest 已通过。本片不启动 JTDX、不连接 CAT/PTT/TX/HIL。
- 收尾边界：HTTP 基础路由与隔离 TCP→Control 测试已完成，但有限 `operations` 摘要/SSE 完成回读尚未接入；`MainWindow::applyWebUiConfiguration()` 强制生产 `enable_frequency_control=false`，设置 checkbox 保留但禁用并说明“完成回读尚未启用”。本片不宣称用户可用 Web 频率写控制。

### P4 频率 HTTP 最小切片结果（2026-09-14）

- 已实现：`JtdxWebServer` 仅接受带显式非空同源 Origin、Bearer、`Content-Length` 的 JSON frequency POST；头/body 有界，拒绝 Transfer-Encoding、Expect、重复长度、管线、畸形 UTF-8/JSON、越界或非 ASCII 整 Hz。频率经生产注入的 `JtdxWebFrequency`/Bands validator 后进入同一个 `JtdxWebControl::submit`，不复制协调器。
- 已实现：配置增加默认关闭的 `WebUiFrequencyControlEnabled` 专用键和设置 checkbox；Service 签名包含该 gate。由于有限 operations/SSE 完成回读尚未接入，MainWindow 强制生产 gate 为 false，checkbox 禁用并提示当前构建未启用，避免只读令牌意外获得写权限。
- 验证：`cmake --build C:\JTDX64\build-webui-dev-msys2 --target jtdx configuration_web_ui_test jtdx_web_server_test --parallel 2` 退出码 0；`QT_QPA_PLATFORM=offscreen` 下 server/config focused tests 均退出码 0；最终 `C:\msys64\mingw64\bin\ctest.exe --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure` 为 `100% tests passed out of 19`，总耗时约 53.39 秒。
- 未验证/未交付：未运行 JTDX、未连接 CAT/PTT/TX/电台、未做浏览器/HIL/部署；有限 `operations` 摘要及其 SSE 完成/超时回读仍是后续批次门槛。本片不宣称用户可用 Web 频率写控制。
- 最终权威证据（结果提交前的文档收尾重建）：`C:\JTDX64\deps-webui\p4-frequency-http-final-build-2ca51ec.log` 明确重建 `jtdx_web_service_test`（含 `JtdxWebService.cpp`、`JtdxWebServer.cpp`）并退出 0；`C:\JTDX64\deps-webui\p4-frequency-http-final-ctest-2ca51ec.log` 为最终全量 CTest `100% tests passed out of 19`、约 53.30 秒。此前 LastTestsFailed/旧 service 二进制不作为最终证据。

### Web 文档同步与 CAT `.10` 交付基线（2026-09-13）

- 任务编号：`JTDX-WEBUI-DOC-SYNC-CAT-20260913`；本批额度：五小时剩余 `45%`、周剩余 `33%`；五小时低于 `30%` 后停止新实现，仅做文档收尾、静态检查和交接。
- 当前基线：仓库 `C:\JTDX64\jtdx_sourcecode`，分支 `main`，HEAD `a89c9da`，工作树干净。`a89c9da` 只补 CAT `.10` 构建与交付文档；本批不改写、回滚或覆盖 CAT 代码或既有 CAT 文档。
- CAT 交付证据：代码提交 `328cc7a` 的错序检测及 artifact code HEAD `9984c38` 的最终 Release，以 [`DEVELOPMENT_STATUS_zh-CN.md`](../../DEVELOPMENT_STATUS_zh-CN.md) 和 [`RELEASE_NOTES_2.2.159.2.10_zh-CN.md`](../../RELEASE_NOTES_2.2.159.2.10_zh-CN.md) 为来源；权威日志 `C:\JTDX64\build-webui-dev-msys2\final-cat-sync-ctest-9984c38.log` 记录完整 CTest `19/19`，含既有 `ftx1_cat_policy_test`，该测试不是本批新增。
- P4/P5 交叉边界：Web DX 只复用 `MainWindow`、AutoSeq/Halt 的既有状态/入口边界，不自行清 DX、重置 CAT/PTT 或复制 recovery。`protocol_sync` 经既有 offline/reconnect，桌面恢复链确认 PTT off 后才清旧 DX；`accepted` 或 DX 短暂为空不能宣称通联结束。重连后新完整解码批次若恢复原 QSO，Web 只呈现实际状态，不抢选 DX。
- 当前 Web 结论与下一步：HTTP 控制仍未接入；下一步仍为 frequency 契约与 HTTP 频率有限切片，当前没有频率 CAT 生产回读或 HIL 证据。P4 DX 与 P5 AutoSeq/CQ 继续沿用上述恢复和状态回读边界。
- 本批动作：只更新本入口、P4-a 契约和本进度日志；不编译、不测试、不运行 JTDX/CAT/PTT/TX/HIL、不部署。收尾执行 `git diff --check`，随后精确创建中文本地 commit，不 push/amend/reset。

### P4 服务/服务器/控制生命周期 epoch 小片开始检查点（2026-09-13）

- 任务编号：`JTDX-WEBUI-P4-LIFECYCLE-20260913`；执行档位：用户指定 `gpt-5.6-luna` / `high`。开局额度：五小时剩余 `97%`、周剩余 `57%`；本批低于五小时剩余 `30%` 时停止新实现，仅做最终验证、精确提交和交接（覆盖先前 `40%` 收尾提示）。
- 基线：仓库 `C:\JTDX64\jtdx_sourcecode`，分支 `main`，HEAD `95d274c`；开始时工作树干净。真实 `jtdx.exe`、CAT/PTT/TX/HIL 均不启动或连接。
- 本批目标：统一 `JtdxWebService`、`JtdxWebServer`、`JtdxWebControl` 的服务 epoch；使 state/SSE 的 `server_epoch` 与控制请求 epoch 一致；服务停止、重启、启动失败、State 销毁及应用退出时使 pending/旧 queued dispatch 失效；已 begin 但未确认的操作保留 `unconfirmed_latch`，重启不得自动解锁。
- 设计边界：默认 disabled 仍拒绝控制；使用 `QPointer` 保护 State/Control 生命周期；复用现有 Control，不重建对象丢失锁；不改 UDP/CAT/TX 调度、不开放 HTTP 控制路由、不新增线程/进程/依赖。
- 文件计划：`JtdxWebControl.hpp/.cpp` 增加停止失效/服务 epoch 绑定接口；`JtdxWebServer.hpp/.cpp` 增加可观测生命周期信号；`JtdxWebService.hpp/.cpp` 持有并绑定 `QPointer<JtdxWebControl>`；`mainwindow.*` 完成唯一对象接线；`tests/jtdx_web_service_test.cpp` 与控制测试覆盖 State/Control/Service 销毁、失败启动、停止重启及旧 queued dispatch；同步本入口与设计/验收文档。
- 恢复点：生命周期接口完成后先构建 `jtdx` 与隔离 Service/Control 测试，再执行一次完整 CTest；命令和日志写入 `C:\JTDX64\deps-webui`，构建目录固定为 `C:\JTDX64\build-webui-dev-msys2`，PATH 以 `C:\msys64\mingw64\bin` 优先、`159\bin` 仅补 Hamlib，测试使用 `QT_QPA_PLATFORM=offscreen`。达到阈值后不再开启新实现。

### P4 服务/服务器/控制生命周期 epoch 小片结果（2026-09-13）

- 已完成：`JtdxWebServer` 报告监听生命周期；`JtdxWebService` 以 `QPointer<JtdxWebControl>` 接收成功监听 epoch，并在停止、启动失败、State 销毁和退出路径触发停止失效；Control 默认未绑定服务，绑定同 epoch 幂等，停止期间拒绝新控制，旧 pending/queued dispatch 不能执行。已 begin 未确认操作在停止/重启后保留 `unconfirmed_latch`；未 begin 的排队请求不建立硬件锁。MainWindow 接入唯一 Control；未开放 HTTP 控制路由。
- 修改文件：`CMakeLists.txt`、`JtdxWebControl.hpp/.cpp`、`JtdxWebServer.hpp/.cpp`、`JtdxWebService.hpp/.cpp`、`mainwindow.cpp`、`tests/jtdx_web_control_test.cpp`、`tests/jtdx_web_service_test.cpp`，以及本入口、设计、验收矩阵和本恢复日志。
- 验证：首轮 Service 测试二进制因目标尚未重编译而出现 restart latch 断言失败；随后以最新源码重建后 targeted `2/2` 通过。最终构建 `cmake --build C:\JTDX64\build-webui-dev-msys2 --target jtdx jtdx_web_control_test jtdx_web_service_test --parallel 2` 退出码 `0`；完整 `ctest --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure` 为 `19/19` 通过、54.83 秒。日志：`C:\JTDX64\deps-webui\p4-lifecycle-final-build.log`、`C:\JTDX64\deps-webui\p4-lifecycle-final-ctest.log`。
- 结果提交：`1c6878e`（`P4：统一 Web 服务控制生命周期 epoch`）。最终工作树干净；未运行 `jtdx.exe`，未连接 CAT/PTT/TX，未做浏览器/HIL/部署。
- 结束额度快照：五小时剩余约 `27%`、周剩余约 `46%`；已低于本片 `30%` 收尾阈值，停止新实现，仅保留本结果与交接。
- 下一步：由根代理只读复核后决定后续批次；本片不扩展 HTTP 控制或真实设备验证。

### P4 频率异步 dispatch gate 小片开始检查点（2026-09-13）

- 额度：本片开局五小时剩余 `100%`、周剩余 `69%`；最新快照五小时剩余约 `43%`、周剩余约 `60%`。本片收尾阈值为五小时剩余低于 `40%`，覆盖历史记录中的 `20%` 门槛。
- 基线：仓库 `C:\JTDX64\jtdx_sourcecode`，分支 `main`，HEAD `dcb429f`；开始时工作树干净。
- 本片目标：把频率排队后的实际执行校验收口到生产 `JtdxWebControl::prepare_dispatch/begin_dispatch`，由 MainWindow 真正调用；补齐 request ID/epoch/pending/deadline/安全/最新 generation 门，防止旧队列在新请求 pending 时执行。
- 契约修正：prepare 前不得完成 CAT feedback；begin 只允许单次消费；排队未执行的 timeout 不设置硬件未确认锁，已 begin 的 timeout 才设置锁；锁存在时同 ID 同 payload 返回原 timeout 记录，新 ID 拒绝；`rotate_epoch` 不清理未确认锁。
- 安全投影：MainWindow 将 rig 在线、monitor、start2、tune、autoTx、transmitting、PTT/IPTT 状态投影到生产 observation，移除重复的 MainWindow gate。
- 文件范围：`JtdxWebControl.hpp/.cpp`、`mainwindow.h/.cpp`、`tests/jtdx_web_control_test.cpp`、本进度日志；不开放 HTTP，不运行 JTDX/CAT/PTT/TX/HIL，不改 159 运行目录。
- 验证计划：`C:\JTDX64\build-webui-dev-msys2` 中构建 `jtdx` 与控制测试，offscreen 环境执行控制测试及一次完整 CTest；命令输出另存 `C:\JTDX64\deps-webui`。
- 实际结果：`cmake --build C:\JTDX64\build-webui-dev-msys2 --target jtdx jtdx_web_control_test --parallel 2` 退出码 `0`，日志为 `C:\JTDX64\deps-webui\p4-dispatch-gate-final-build.log`；控制测试通过，日志为 `C:\JTDX64\deps-webui\p4-dispatch-gate-test.log`。
- 全量回归：`ctest --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure` 为 `100% tests passed out of 19`，总耗时约 `53.33s`，退出码 `0`；日志为 `C:\JTDX64\deps-webui\p4-dispatch-gate-final-ctest.log`。
- 本片完成：`prepare_dispatch` 在排队执行时重读生产 observation、检查 request ID/epoch/pending/deadline/安全及频率 CAT 基线，`begin_dispatch` 需持有 prepare 标记并校验 generation 后单次消费；MainWindow 已实际调用，不再复制控制 gate。未 begin 的 feedback、旧队列和重复 begin 均不能完成或执行。
- 锁与幂等：仅已 begin 的超时设置 `unconfirmed_latch`；排队未执行超时允许后续新 ID，超时同 ID 同 payload 返回原记录；epoch 轮换保留未确认锁。未知 rig/monitor 默认 fail-closed，MainWindow 投影 rig/monitor/start2/tune/autoTx/transmitting/PTT/IPTT。
- 未运行：真实 `jtdx.exe`、CAT/PTT/TX/HIL、浏览器/HTTP 控制、部署；未修改 `C:\JTDX64\159` 运行目录。
- 结果提交：待精确暂存本片源码、测试和恢复日志后创建本地中文 commit，不 push/amend/reset。

### P4 频率纯校验与生产适配批次开始检查点（2026-09-13，当前批次）

- 任务编号：`JTDX-WEBUI-P4-FREQUENCY-20260913`；执行档位：用户指定 `gpt-5.6-luna` / `high`。开局额度：五小时剩余 `89%`、周剩余 `83%`；当前收尾阈值改为五小时剩余低于 `20%`。历史批次中的 `30%` 仅作历史记录，不覆盖本批门槛。
- 基线：仓库 `C:\JTDX64\jtdx_sourcecode`，分支 `main`，HEAD `8055c58`；开始时工作树干净。现用 `C:\JTDX64\159\bin` 中的 `jtdx.exe`/`jtdxjt9.exe` 仅作外部状态参考，不触碰、不启动真实 JTDX、不连接 CAT/PTT/TX/HIL。
- 本批目标：实现频率普通控制的严格纯解析/范围校验，并为后续 MainWindow 生产适配保留可审查接口；最终完成条件仍是基于真实 `observe_rig`/`rig_generation` 的 CAT 回读。当前不开放 HTTP 控制、不统一 WebServer/Control epoch、不连接真实 CAT。
- 安全边界：不得复用 `LiveFrequencyValidator` 控件 Intermediate 语义；不得把 nominal/target、HTTP 200 或全局 revision 当作 CAT 完成；不得直接调用 PTT/TX；不得新增 UDP、线程、进程、依赖；生产适配必须保留 `band_changed` 的既有副作用语义并先经根代理只读审查。
- 根代理已指出的协调器待补门：provider 可重入需有 `submit_in_progress` 保护；provider 二次异常/epoch 变化不得错误返回新 epoch 记录；reject 结果应带时间/快照；QTimer 真实事件循环超时需补隔离验证。本批若触及协调器，仅做最小必要修复并单独记录。
- 文件计划：新增/复用纯频率策略文件与隔离单元测试；必要时增加 `JtdxWebControl` 严格频率校验及接口声明；不改 `JtdxWebServer` HTTP 路由、不直接接入 MainWindow，除非形成独立可验证切片。
- 恢复点：每个小片完成后记录基线/结果提交、修改文件、命令与退出码；只使用 `C:\JTDX64\build-webui-dev-msys2` 及 `C:\JTDX64\deps-webui`，构建环境 `C:\msys64\mingw64\bin` 优先、`159\bin` 仅补 Hamlib、`QT_QPA_PLATFORM=offscreen`。低于 `20%` 后停止新实现并交接。
- 当前结果：新增 `JtdxWebFrequency` 纯策略，严格拒绝空值、符号、空白、小数、指数、非 ASCII 数字、溢出、零值和 OOB，已复用 `Bands::find`；`JtdxWebControl` 增加 provider 重入保护、reject 时间/缓存快照、二次 provider epoch 错误隔离、超时未确认门和显式 `fail`。
- MainWindow 适配：构造时注册 control observation provider 和 queued frequency dispatcher；执行时再次检查 epoch/pending、状态新鲜、Rig 在线、monitor、`m_start2`/tune/QuickCall/TX/IPTT，设置 `m_bandEdited` 后复用 `band_changed` 并更新 WideGraph；`handle_transceiver_update` 仅在在线、PTT 关闭且安全状态新鲜时用真实 `rig_generation`/revision 回读。未接 HTTP，WebServer epoch 尚未统一。
- 自动验证：`jtdx_web_frequency_test`、`jtdx_web_control_test` 均通过（2/2）；`cmake --build C:\JTDX64\build-webui-dev-msys2 --target jtdx` 通过。未运行真实 JTDX，未连接 CAT/PTT/TX/HIL；MainWindow 适配未做完整窗口构造或设备回读验证，不能据此宣称频率控制已生产可用。
- 根代理最终全量 CTest：`19/19` 通过，耗时 `55.97s`，退出码 `0`；收尾额度为五小时剩余 `8%`、周剩余 `70%`，已按低于 `20%` 规则暂停新实现。
- 结果提交：源码实现结果提交为 `790c86c`；最终全量 CTest 的权威结果以 `C:\JTDX64\build-webui-dev-msys2\Testing\Temporary\LastTest.log` 为准。实际构建命令为 `cmake --build C:\JTDX64\build-webui-dev-msys2 --target jtdx jtdx_web_control_test jtdx_web_frequency_test --parallel 2`；测试命令为 `ctest --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure`，运行环境为 `C:\msys64\mingw64\bin` 优先、`C:\JTDX64\159\bin` 仅补 Hamlib、`QT_QPA_PLATFORM=offscreen`。
- 待根代理只读验收：`unconfirmed_feedback` 锁目前只由 `rotate_epoch` 清除，不能由后续 Server epoch 统一或重启自动绕过未确认硬件状态；超时后同 request ID 会先被 latch 拒绝而不会返回原记录；异步 dispatcher 的 baseline generation 仍是登记时值。HTTP 接入前必须针对这些契约补齐并测试。
- 下一步：根代理只读审查后，再决定是否将 HTTP frequency 请求接入统一 server/control epoch；接入前不得开放控制路由。

### P4-b 控制协调器基础批次开始检查点（2026-09-10，当前批次）

- 任务编号：`JTDX-WEBUI-P4-B-COORDINATOR-20260910`；执行档位：用户明确指定 `gpt-5.6-luna` / `high`。额度开局记录：五小时剩余 `92%`、周剩余 `21%`；低于五小时 `30%` 后只做收尾、静态检查、精确提交和交接。
- 基线：仓库 `C:\JTDX64\jtdx_sourcecode`，分支 `main`，`5611fcc`；开局 `git status --short --branch` 干净。现用 `jtdx.exe`/`jtdxjt9.exe` 不属于本批目标，不触碰、不启动真实 JTDX、不连接 CAT/PTT/TX/HIL。
- 本批目标：新增不激活的 `JtdxWebControl` 主 Qt 事件循环协调器与隔离单元测试；支持已规范化的 frequency/select-dx 两类登记、有限状态、幂等/409、epoch/shutdown、单调超时、有界记录和显式业务 dispatch/feedback 接口。协调器不拥有 QWidget、不连接 MainWindow、不直接 CAT/PTT、不新增 UDP/线程/进程/依赖。
- 完成门：frequency 只能以对应来源的新 CAT generation 与目标实际频率回读完成；select-dx 只能以协调器独立的 DX feedback generation 与目标 Call/Grid 完成。全局 `state_revision`、HTTP 200、预写目标字段或旧快照均不能完成。当前不声称已有 Calls/DX 业务入口或生产控制接入。
- 受控设计（先供根代理审查）：单一在途操作；不同 request 在途时 `rejected/busy`，不自动排队；同 ID 同规范 payload 返回首次记录，不重做；同 ID 不同 payload 返回 `409/request_id_conflict`。epoch 内记录达到 hard limit 后 fail-closed；仅在无 pending 时显式轮换 epoch 并清理旧记录，旧 ID 不得复活。dispatch 前通过安全状态提供器重新读取，未知/过期/ transmitting/PTT/Enable Tx/watchdog/业务状态均拒绝。
- 文件计划：`JtdxWebControl.hpp/.cpp`、`JtdxWebState.hpp/.cpp`（仅 rig generation）、`tests/jtdx_web_control_test.cpp`、`CMakeLists.txt`，并同步本入口、`docs/web-ui/PROGRESS_zh-CN.md`、`docs/web-ui/02_设计_API_安全_zh-CN.md`、`docs/web-ui/03_阶段验收矩阵_zh-CN.md`。不改 `JtdxWebServer`、`MainWindow`、UDP/CAT/桌面入口。
- 测试计划：纯协调器安全门、同步 feedback 竞态、generation/target 匹配、旧 generation/全局 revision 误完成防护、幂等冲突、busy、超时/迟到 feedback、epoch/shutdown、hard limit；随后按既定环境以 `mingw64/bin` 优先、`159/bin` 仅补 Hamlib、`QT_QPA_PLATFORM=offscreen` 构建 jtdx、目标测试并执行一次全量 CTest。日志写入 `C:\JTDX64\deps-webui`。
- 恢复点：若额度低于收尾阈值或测试受阻，保留本检查点与当前 diff，记录命令/退出码/日志，不覆盖既有修改；完成后先给根代理只读 review，再形成精确中文本地 commit，不 push/amend/reset。
- 当前结果：`jtdx_web_control_test` 与 `jtdx_web_state_test` 隔离运行通过；随后新增 provider epoch 重入、feedback 后 rotate/throw 且同 ID 新 epoch pending 保持测试并通过。初轮全量 CTest 在补测前为 `18/18`（55.15 秒），日志 `C:\JTDX64\deps-webui\p4b-final-ctest.log`；补测后重建 `jtdx` 与单跑控制测试，日志 `C:\JTDX64\deps-webui\p4b-final-reentrant-build.log`。未重复全量 CTest，补测新增路径需以后续全量回归再确认。

### P4-a 普通控制实施契约准备（2026-09-10，历史契约；当前实现状态见 P4-b）

- 基线：`main` / `1ef3804`；开始检查时工作树干净。P3 生产 `JtdxWebService`、真实 `QAction` 菜单入口和 17 项自动测试已完成；完整 `MainWindow` 窗口构造/人工验收未完成，留到 P6。
- 本批范围：只核对 DX 选择/Call 校验的现有入口、副作用、安全状态门、业务回读完成条件，冻结 DX/Call 子契约、文件清单、测试计划和恢复检查点；frequency 完整请求结构、文件清单和测试契约留下一批。不改生产代码/测试，不开放 HTTP control，不启动 JTDX，不连接 CAT/PTT/TX/电台，不做浏览器/HIL。
- 关键事实：当前没有 `CallsignValidator`；`QRegularExpressionValidator` 只提供字符集门。`Radio::is_callsign()`（`Radio.cpp:86-94`）在长度检查前访问 `at(1)`，Web 适配必须先挡空值/短输入。`doubleClickOnCall/processMessage`（`mainwindow.cpp:5604-5973`）会改周期、QSO/Tx 消息并在 AutoTx 下点击 Enable Tx，绝不能作为只选择 DX 的 Web 入口。
- 回读门：`JtdxWebState` 当前是全局 revision；status/rig/decode 观测都会递增，不能凭 revision 增长证明 DX 应用完成。P4 实现前必须增加 DX 专属 generation/source event，或使用等价可审计业务回读标识；相同目标须标识 `already_selected`，不能伪装新 CAT/DX 事件。
- 文档：详细契约见 [`P4-A_普通控制实施契约_zh-CN.md`](P4-A_普通控制实施契约_zh-CN.md)。P4 仍未实现，本文不构成控制完成或 HIL 证据。
- 额度：本批中途快照五小时剩余 `36%`、周剩余 `25%`；末次收尾快照五小时剩余 `30%`、周剩余 `24%`。已进入 30% 收尾阈值，后续只做有界文档/静态检查/精确提交/交接。

### P3 MainWindow 菜单动态验收恢复点（2026-09-10，历史批次）

- 基线：`main` / `30eeb24`；工作树恢复时干净。当前只收尾 P3 MainWindow 菜单入口动态验收，Configuration QWidget 隔离动态验收已完成 `16/16`；不进入 P4。
- 额度规则：本阶段开局账户共享五小时剩余 `98%`、周窗口剩余 `35%`；`30%` 是本阶段收尾阈值。此前文档中的 `40%` 和 `20%` 门槛均废止；低于 `30%` 后只允许有界文档、静态验证、精确提交和交接，不开启新的实现面或长时间动态运行。
- 用户进程边界：现用 `jtdx.exe` PID `9260`、`jtdxjt9.exe` PID `24568` 位于 `C:\JTDX64\159\bin`；不得操作窗口、关闭进程、修改其配置或连接设备。
- MainWindow 事实：构造会创建既有音频/解码/CAT 相关对象，启动 `m_guiTimer`，并无条件启动 `jtdxjt9`；随后还会排队 `rigOpen()`。因此完整 MainWindow 构造不是本批安全动态证据，不能用假 fixture 复制 Web 方法替代。
- 已实施的最小路线：提取只承载生产 Web 生命周期（应用配置、重复启动/停止、失败恢复、epoch/端口状态、退出清理）和 URL opener 的 `JtdxWebService`，由生产 `MainWindow` 持有、转换 Configuration 快照并由真实 `QAction` 连接调用；测试只实例化该生产对象与真实 QAction 入口，使用 `QDesktopServices::setUrlHandler` 捕获默认 URL。未复制 CAT、PTT、TX、AutoSeq、音频、解码或 UDP 逻辑，未新增生产线程、进程、UDP listener 或控制 API。
- 已执行：`jtdx_web_service_test` 在 `QT_QPA_PLATFORM=offscreen` 下通过；覆盖默认关闭、QAction→生产 `open()`、默认 URL handler 捕获、失败 opener、重复打开同 URL/epoch/port、不重复启动、停止后新 epoch、占用端口失败及释放后恢复、错误清除、排队 open 在 shutdown 后拒绝、shutdown 后 apply 拒绝和析构释放 TCP 端口。该证据覆盖 Web 菜单业务路径，不等同于完整 MainWindow 构造或人工点击。
- 交付验证：`cmake --build C:\JTDX64\build-webui-dev-msys2 --target jtdx --parallel 2` 退出码 `0`，日志 `C:\JTDX64\deps-webui\web-service-jtdx-final-build.log`；`ctest --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure` 为 `100% tests passed out of 17`，总耗时约 `54.69s`，日志 `C:\JTDX64\deps-webui\ctest-webui-p3-menu-service.log`；新测试单独 CTest 为 `1/1`，日志 `C:\JTDX64\deps-webui\ctest-web-service.log`。
- 本批修改文件：`JtdxWebService.hpp/.cpp`、`mainwindow.h/.cpp`、`CMakeLists.txt`、`tests/jtdx_web_service_test.cpp`、本进度日志和 `WEB_UI_P3_验收_zh-CN.md`；准备一个本地中文 Git 提交，不 push、不 amend。
- 当前未执行：真实 JTDX、CAT/PTT/TX/HIL、部署、message_aggregator 和长时间浏览器耐久性；Configuration 重载/失败恢复已在前一切片完成。构建继续使用 `build-webui-dev-msys2`/`deps-webui` 现有 Hamlib，运行时 `mingw64/bin` 优先、`159/bin` 仅补 Hamlib，`QT_QPA_PLATFORM=offscreen`。

### P3 动态 Configuration 恢复批次检查点（2026-09-09）

- 基线：`main` / `6b500ef1890fbe666529c16ce0c95dc75aeea9f1`；当前工作树保留上一 Luna 同任务修改：`CMakeLists.txt`、`Configuration.cpp`、`docs/web-ui/PROGRESS_zh-CN.md`、`tests/configuration_web_ui_test.cpp`，不得覆盖。
- 本批目标：完成真实 `Configuration` QWidget 的隔离动态验收及最小缺陷修复；先不展开 MainWindow 菜单新切片，不进入 P4。
- 开局额度：五小时窗口剩余 `100%`，周窗口剩余 `49%`。新收尾门槛：五小时窗口剩余低于 `40%` 时立即停止新实现，只完成文档、验证、精确提交和交接；此前 `20%` 门槛废止。
- 安全边界：仅使用隔离 `QSettings`、临时数据目录、`Rig=None`、空 CAT/PTT/网络和关闭 TX/QuickCall/AutoSequence 的测试夹具；不启动完整 JTDX，不附着或操作现用 `jtdx.exe`，不进行 CAT/PTT/TX/HIL/部署，不新增生产 UDP/线程/process/控制。
- 已有状态：测试可执行文件曾编译成功但运行尚未确认；`deps-webui/configuration-test-build.log`、`stderr`、`entry.log`（仅进入 `main` 两次）不是 pass 证据。当前优先定位 `QApplication` 构造前后退出及 Qt 平台插件路径，再核对 Configuration 测试。
- 当前恢复动作：保留所有现有 diff，先做 Qt 路径/插件诊断和最小探针；确认后再修 `QSettings` 分组、`settings_snapshot`、5 秒 timer 生命周期、磁盘重载和非法端口持久值等本批明确问题。每次暂停前补齐命令、退出码和日志路径。

### P3 动态 Configuration 收尾结果（2026-09-09）

- 五小时额度收尾读数：剩余 `29%`，已触发本批规定的 `<40%` 门槛；不再开启菜单新切片。
- 已完成：真实 `Configuration` QWidget 隔离夹具的默认值、取消、确定、重复打开、磁盘重载、令牌摘要持久化、Rig=None/CAT 离线和非法持久端口 `77881` 拒绝并关闭 Web UI；补齐测试夹具所需的 `JTDXDateTime`，未改 CAT 生产逻辑。
- 已修复：测试 deadline 改为绑定 `Configuration` 的栈 `QTimer`，在 `exec()` 返回前停止，避免旧 singleShot 引用跨对话生命周期；取消断言排除 `Configuration::done()` 已有的窗口几何键写入，只比较临时业务设置。
- 最终构建：`cmake --build C:\JTDX64\build-webui-dev-msys2 --target jtdx --parallel 2`，退出码 `0`；日志 `C:\JTDX64\deps-webui\configuration-final-jtdx-build.log`。测试目标构建日志为 `C:\JTDX64\deps-webui\configuration-final-build.log`。
- 最终回归：`ctest --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure`，`100% tests passed out of 16`，总耗时约 `54.04s`；日志 `C:\JTDX64\deps-webui\ctest-webui-p3-configuration-final.log`。CTest 使用 `QT_QPA_PLATFORM=offscreen`，运行环境以 mingw Qt 优先、159/bin 仅补 Hamlib 依赖。
- 尚未验证：MainWindow 菜单动态重复点击、真实隔离 `jtdx.exe`、CAT/PTT/TX/HIL、部署和长期浏览器耐久性；非法持久端口已验证失败关闭，但本批未新增专门的界面错误提示。

### P3 动态验收批次开始检查点（2026-09-09）

- 基线：`main`、`6b500ef`；工作树开始时干净。执行档位按用户要求为 `gpt-5.6-luna`、`high`；本批不升级依赖。
- 本批目标：在隔离 `QSettings`/临时数据目录、`Rig=None`、CAT/网络/PTT 为空且 TX/QuickCall/AutoSequence 关闭的条件下，使用真实 `Configuration` QWidget 验收保存、取消、重载、重复打开和 Web UI 端口/令牌持久化；在可安全构造的范围内验收 MainWindow 菜单重复点击、服务 epoch/端口保持及异常隔离。
- 安全路线：先运行 Qt Widgets 动态夹具；不附着、停止或修改现用 JTDX（现用实例由父任务核实），不启动真实 CAT/PTT/TX，不使用用户配置、日志或部署路径，不新增生产线程/process/UDP/调度。
- 计划验证：默认关闭/loopback、自动与手动端口及冲突、取消不写入、确定后重载一致、令牌只保存摘要、重复打开不泄漏服务/端口、Web 异常不影响应用、退出清理。只修本批动态验收发现的 P3 缺陷。
- 当前状态：已完成文档/源码路线复核；动态夹具、测试命令、缺陷修复和结果尚未执行。

## P3 批次记录（2026-09-09）

- 基线：`main`、`ed04be6`；工作树在本批开始时干净。执行模型为 Luna high；本批开局额度记录为 5 小时、周额度 86%。
- 本批目标：完成 P3 配置 Tab、MainWindow 唯一服务生命周期、Qt Resource 原生只读页面、菜单入口及对应夹具/回归测试；不进入 P4/P5 控制。
- 已冻结文件清单：`Configuration.hpp/.cpp/.ui`、`mainwindow.h/.cpp/.ui`、`JtdxWebServer.hpp/.cpp`、`CMakeLists.txt`、`tests/jtdx_web_server_test.cpp`、`tests/ui_contract_test.cpp`、`resources/web-ui/index.html`、`style.css`、`app.js`、`docs/web-ui/WEB_UI_P3_验收_zh-CN.md`、本日志及入口文档。
- 已完成：确认 P2 `JtdxWebServer`/`JtdxWebState` 接口；冻结 Web 专用持久键、自动端口排除 UDP 数字、仅本机/明确 LAN 地址、SHA-256 令牌摘要和零新线程/进程/UDP 边界；开始把服务鉴权改为摘要恒定时间比较。
- 已完成：配置 Tab 与 MainWindow 接入、正式资源路由和前端 SSE、P3 服务/资源回归、独立构建/CTest、浏览器夹具复验、日志留档和事实文档；动态 Configuration QWidget 保存/取消/重载/重复打开及菜单重复点击留待后续 P3 验收。
- 安全边界：令牌原文只在设置对话框的临时编辑控件和用户浏览器内存中出现；不得进入 QSettings、URL、HTTP HTML、日志或服务成员。应用/HIL/真实 CAT/PTT/TX 尚未启动，浏览器验证需先使用隔离夹具。

### P3 实施进展 checkpoint（2026-09-09，历史检查点；最终结果见下节）

- 已完成：配置 Tab/专用持久键、摘要令牌、MainWindow 唯一生命周期、菜单/打开/重启按钮、Qt Resource 页面、fetch-SSE 令牌输入/断线/epoch ID/未知值处理、浏览器夹具 `--serve-browser`。
- 已验证：`C:\JTDX64\build-webui-dev-msys2` 全量构建成功，权威日志为 `C:\JTDX64\deps-webui\build-webui-p3-final.log`；`ctest --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure` 实际结果 `100% tests passed out of 15`，权威日志为 `C:\JTDX64\deps-webui\ctest-webui-p3-final.log`。
- 已收尾：浏览器夹具带演示解码和陈旧状态复验、`git diff --check`、中文事实文档和精确本地提交均已完成。未做动态 Configuration 对话框保存/取消/重载/重复打开、菜单重复点击、真实 JTDX 隔离启动、CAT/PTT/TX/HIL、部署。

本文件是阶段交接记录。每次继续工作先更新“本次批次”表，再开始代码或测试；每次暂停前必须补齐命令和证据。不要把未来计划写成已完成事实。

### P3 最终收尾检查点（2026-09-09）

- 最终二进制已重建：`cmake --build C:\JTDX64\build-webui-dev-msys2 --parallel 2`，退出码 `0`；日志 `C:\JTDX64\deps-webui\build-webui-p3-final.log`。
- 最终二进制全量回归：`ctest --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure`，`100% tests passed out of 15`，退出码 `0`；日志 `C:\JTDX64\deps-webui\ctest-webui-p3-final.log`。
- 浏览器夹具已用最终二进制启动：`http://127.0.0.1:49152/#fixture`；令牌仍只由进程标准输出一次提供。夹具包含 6 条带 DF、网格、fresh/is_new 和 `<script>` 文本的演示解码，7 秒后推进单调时钟越过 5 秒新鲜度阈值，并设置有界 120 秒退出。父任务复验确认桌面/390px 移动布局、最新优先解码、陈旧中文提示、空实际频率、AutoSeq/TX 文本和重复连接单 TCP 流；断线后的旧快照/陈旧标识由只读代码检查和错误令牌断线观察支持。未在 120 秒自动退出瞬间观察页面；父任务已确认夹具进程不存在。
- 前端最终修订：显示业务状态更新时间、解码年龄、实际频率 `rig_fresh` 状态、AutoSeq、DF、每条解码新鲜度/新旧标识；使用 `textContent`；fragment 演示标识不进入 HTTP；连接循环有 runId、局部 AbortController、每次读取看门狗和旧快照陈旧标识。
- 尚未验证：真实隔离 `jtdx.exe`、Configuration QWidget 动态保存/取消/重载/重复打开及菜单重复点击、CAT/PTT/TX/HIL、部署和长期浏览器耐久性。P3 代码、资源、测试和文档完成后即停，不进入 P4。

## P3 历史恢复点（不覆盖当前 P4-a）

- 当前阶段：`P3 设置/菜单/只读前端`（代码、资源、构建、CTest 和夹具浏览器复验已完成；动态 QWidget 设置/菜单交互待后续 P3 验收）。
- P2 历史状态：已完成；P1 已完成（源码结果提交 `0420a21`，文档结果提交 `b788d0e`，收尾提交 `e27c9bb`）。P3 工作树起始基线为 `ed04be6`；本批结果以单一本地中文提交记录。
- 基线分支/提交：`main` / `ed04be6`（P3 批次开始实际读数）。
- P0 初始结果提交：`02153b0`（提交后修订不 amend，使用 `git log --oneline -- docs/web-ui` 追踪后续文档提交）；提交后必须用 `git show --stat --oneline HEAD` 和 `git status --short --branch` 复核。
- 工作树预期：本批结束前列出并精确提交本批修改；不得覆盖其他工作。
- 模型策略：按用户要求使用 `gpt-5.6-luna`、`high`；不自行升级模型或推理档位。
- 配额策略：本批开始账户共享快照为 5 小时剩余 93%、周剩余 80%；本批收尾快照（2026-09-08）为 5 小时剩余 26%、周剩余 70%。这些是账户共享读数，不是本任务独占额度；当前仍高于新批阈值 20%，但本批不再扩展范围，只收尾并写恢复记录。本任务不创建定时任务或自动化监控。

## P2 本批记录（进行中 checkpoint）

### 2026-09-08 P2 批次开始检查点

- 开始额度：账户共享 5 小时/周窗口均剩余 `100%`；本批额度低于 `20%` 时不再开启新批，只收尾并记录恢复点。
- 基线：`main` / `e27c9bb`，工作树开始时干净；代码根目录 `C:\JTDX64\jtdx_sourcecode`。
- 本批目标：在主 Qt5 事件循环内增加惰性、默认不监听的 `JtdxWebServer`，仅提供只读首页、`GET /healthz`、`GET /api/v1/state`、`GET /api/v1/decodes` 和 SSE `GET /api/v1/events`；严格限制 GET/HTTP/1.1、Host/Origin、Bearer 鉴权、连接/头/事件/队列、端口生命周期和 epoch/revision 重同步。
- 明确边界：不自动启动、不启动 JTDX、不连接 CAT/PTT/TX/HIL、不新增线程/进程/UDP、不改现有 UDP/CAT/解码逻辑；P3 设置/菜单/正式前端与 P4 控制均不在本批。
- 文件计划（先冻结后改动）：`JtdxWebServer.hpp`、`JtdxWebServer.cpp`、`CMakeLists.txt` 最小源文件接入；`tests/jtdx_web_server_test.cpp` 真实 loopback TCP 夹具；本文件及 `03_阶段验收矩阵_zh-CN.md`/`WEB_UI_START_HERE_zh-CN.md`/必要设计文档按事实同步。必要时只在 `mainwindow.*` 增加惰性持有/测试注入接口，默认不监听。
- SSE 尺寸检查：在测试中实测 500 条完整 decode JSON；采用实测依据选择单事件与单客户端待发送上限，确保合法满 500 快照首次 HTTP/SSE 成功，并验证慢客户端在有界背压下断开；无 revision 变化时通过周期性 snapshot/新鲜度投影保持可见状态更新。`server_epoch`/`web_server_state` 由服务投影，不篡改业务新鲜度。
- 验收计划：真实 loopback TCP 覆盖自动/手动/占用/UDP 数字排除/停止重启/重复启停、JSON/API、鉴权/Host/Origin、malformed/slow、SSE reconnect+epoch/背压/多客户端；再跑原 14 项 CTest。未运行 `jtdx.exe`、真实 CAT/PTT/TX/HIL/部署。

### 2026-09-08 P2 批次结果（已完成）

- 已实现：`JtdxWebServer.hpp/.cpp` 使用 Qt5 `QTcpServer`/`QTcpSocket`，只运行在创建线程的 Qt 事件循环；默认惰性、不自动监听；加入 `wsjt_qt`，但未在 `MainWindow` 自动创建或启动。`QPointer` 状态生命周期保护，State 销毁停服。
- HTTP 合同：仅 `GET`/HTTP/1.1；严格 CRLF、Host/Origin、无 query/body/TE/重复头；头上限 16KiB、头时限 5 秒、读取缓冲有界；首页仅无敏感引导；`/healthz`、state、decodes、SSE 均要求 bearer；所有控制 POST 路径不执行并返回 404/405；响应 drain 有界，异常只关闭当前连接。
- 端口和安全：默认绑定 `127.0.0.1`；自动端口仅 49152..49251，排除传入 UDP 数字且不执行 UDP bind；手动端口 1024..65535；LAN 仅允许显式具体地址、强令牌和匹配的 HTTP Origin；令牌不进入 URL/响应/日志。
- SSE：snapshot 的 `server_epoch`/`web_server_state` 是服务层投影，业务 revision/freshness 不被伪造；每 2 秒周期 snapshot 使无 revision 变化的新鲜度仍可观察，另有 heartbeat；`Last-Event-ID` 超限拒绝，历史不保留时发送 `resync_required` 和完整 snapshot；事件上限 1MiB、单客户端待发送上限 1MiB，慢连接服务端回收。500 条长文本 decode 的实测完整 snapshot 约 `278 KiB`（`/api/v1/decodes` 双字段 HTTP body 约 `554 KiB`），因此事件上限未误设为 512KiB。
- 测试文件：`tests/jtdx_web_server_test.cpp` 真实 loopback TCP 覆盖自动/手动/占用/UDP 数字排除、停止/重启/epoch、重复启动、首页/health/state/decodes、满 500 JSON 读回、令牌/Host/Origin/query/body/POST、CRLF/部分头超时、SSE 完整初次/重同步、连接上限及恢复、慢 SSE 背压回收、State 销毁停服和 timer/socket 生命周期。
- 最终构建命令：`C:\msys64\mingw64\bin\cmake.exe --build C:\JTDX64\build-webui-dev-msys2 --parallel 2`，主目标 `jtdx.exe` 与 `jtdx_web_server_test.exe` 均链接完成；退出码 `0`，日志：`C:\JTDX64\deps-webui\build-webui-p2-final.log`。
- 最终测试命令：`C:\msys64\mingw64\bin\ctest.exe --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure`；最终 `100% tests passed out of 15`，退出码 `0`，日志：`C:\JTDX64\deps-webui\ctest-webui-p2-final.log`。此前一次全量运行因慢连接测试等待窗口不足报告 `14/15`；最终夹具观察窗口已延长，慢 SSE 服务端回收和其余 15 项均通过。
- 未运行：`jtdx.exe`、真实 CAT/PTT/TX/HIL、浏览器、部署；P3 设置/菜单/正式前端与 P4/P5 控制仍未开始。
- 收尾额度：账户共享窗口约 5 小时剩余 `29%`、周剩余 `89%`；本批不再开启新阶段，等待用户检查 P2 提交。

## P1 本批记录

### 2026-09-08 P1 批次（已完成；以下先记录开始检查点）

- 开始额度：账户共享 5 小时窗口剩余 `93%`（used `7%`），周窗口剩余 `80%`（used `20%`）；低于 `20%` 时停止新批，只收尾并写恢复点。额度读数来自本批开始的实时查询。
- 本批目标：在既有 `JtdxWebState`/`MessageClient`/`MainWindow` 只读接入上补足有界 P1：复用现有 `DecodedText` 的 Call/Grid 事实，补 `auto_sequence_state`、`cq_state`、`current_tx_text` 的内部业务字段快照，保持单调新鲜度、应用在线与 CAT 在线分离且无控制副作用；增加 live decode fresh→stale、WSPR `>UINT32_MAX`、UDP 空/disabled 仍观察且 mirror 不重复、clear/状态 JSON 合同测试。
- 明确边界：不做 P2 TCP server/API/SSE，不新增 UDP 监听、线程、进程、控制、设置、菜单；不重复解析 UDP，不从控件反读业务状态；不启动 `jtdx.exe`，不连接 CAT/PTT/TX，不做 HIL 或部署；允许运行既有 loopback UDP 测试夹具，但它不是生产 Web 新监听。
- 文件计划（先事实后最小修改）：`JtdxWebState.hpp/.cpp`、现有 `MessageClient.*`/`mainwindow.*` 只读接入点、P1 测试源、必要文档；Hamlib 只在独立 `deps-webui`/`build-webui-dev-msys2` 使用，不全局安装/升级、不替换现用运行库。
- 验收计划：Qt-only P1 单测与既有 UDP loopback 夹具先行；恢复匹配 Hamlib 后独立配置/构建和完整 CTest。严格区分静态检查、Qt-only 单测、完整构建/CTest、应用/HIL/部署；完整配置失败时不得运行旧 exe 掩盖失败。

### P1 构建前置探针与恢复结果（已完成）

- `C:\msys64\mingw64\bin\cmake.exe` 4.4.0、GCC/G++/GFortran 16.1.0、Ninja 均可用；MSYS2 bash 中显式加入 `/c/Windows/System32` 后，C/C++/Fortran 编译器识别和 ABI 探针通过。
- `C:\msys64\mingw64\bin\cmake.exe` 4.4.0、GCC/G++/GFortran 16.1.0、Ninja 均可用；MSYS2 bash 中显式加入 `/c/Windows/System32` 后，C/C++/Fortran 编译器识别和 ABI 探针通过。
- 官方 Hamlib 4.7.2 release 资产 `C:\JTDX64\deps-webui\hamlib-4.7.2.tar.gz` SHA256=`AE1FCF2DBC80EA0786EA8F047B09399C3F7737D1930442F61A031708ED33E88F`；现用 `C:\JTDX64\159\bin\msys-hamlib-4.dll` SHA256=`630A90E02F56E0D5F02A77E8D172F61F041399900DBFFA57A4FB989896895DB3`。从该现用 DLL 生成独立导入库 `C:\JTDX64\deps-webui\libhamlib-4.7.2-from-existing-dll.a`，没有重新编译或替换现用 DLL。
- 4.7.2 头文件静态 ABI 探针 `C:\JTDX64\deps-webui\evidence\history\abi_probe.cpp` 通过：`sizeof(rig_state)=31416`、`offsetof(tx_vfo)=13364`、`offsetof(vfo_list)=13312`、`offsetof(pttport)=14840`、`sizeof(s_rig)=47752`、`sizeof(rig_caps)=26072`。
- 独立 CMakeCache 已读回 `Hamlib_INCLUDE_DIR=C:/JTDX64/deps-webui/hamlib-4.7.2/include`、`Hamlib_LIBRARY=C:/JTDX64/deps-webui/libhamlib-4.7.2-from-existing-dll.a`、`WSJT_ENABLE_OMNIRIG=OFF`、`WSJT_GENERATE_DOCS=OFF`；首次配置仅因缺 `asciidoctor` 停止，关闭文档生成后配置成功并显示 `Found Hamlib`。
- 配置命令：`C:\msys64\usr\bin\bash.exe -lc 'export PATH=/mingw64/bin:/usr/bin:/c/Windows/System32:/c/Windows; cmake -G Ninja -S /c/JTDX64/jtdx_sourcecode -B /c/JTDX64/build-webui-dev-msys2 -DCMAKE_BUILD_TYPE=Release -DJTDX_BUILD_LOCAL_TESTS=ON -DWSJT_ENABLE_OMNIRIG=OFF -DWSJT_GENERATE_DOCS=OFF -DCMAKE_C_COMPILER=/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=/mingw64/bin/g++.exe -DCMAKE_Fortran_COMPILER=/mingw64/bin/gfortran.exe -DCMAKE_MAKE_PROGRAM=/mingw64/bin/ninja.exe -DHamlib_INCLUDE_DIR=/c/JTDX64/deps-webui/hamlib-4.7.2/include -DHamlib_LIBRARY=/c/JTDX64/deps-webui/libhamlib-4.7.2-from-existing-dll.a'`；日志：`C:\JTDX64\deps-webui\configure-webui.log`。

### P1 当前成果与限制

- 已实现：`JtdxWebState.hpp/.cpp`、主 `MessageClient` 本地观察信号、`MainWindow` 的状态/解码/clear/CAT 实测频率只读接入；不增加 UDP/TCP、线程、进程、控制、设置或菜单。UDP 目标为空或关闭时仍先发出本地观察；secondary mirror 不重复发送相同目标的结构化包。
- 已覆盖：空状态未知/null、状态和 live decode fresh→stale、CAT offline 与 PTT 未确认、目标/实测频率分离、默认 300/硬 500 解码上限、最旧淘汰、clear/revision、JSON 类型、replay 不增长、不刷新、off-air 不刷新、实例 ID、WSPR 频率超过 `UINT32_MAX`；FT8 Call/Grid 复用生产 `DecodedText::deCallAndGrid` 结果，不新增正则；内部 AutoSeq/QSO/TX 文本以 `auto_sequence_state`、`qso_stage`、有 CQ 选中条件的 `cq_state`、`current_tx_text` 输出。
- UDP 合同测试已覆盖空目标和 disabled 客户端仍发出本地 `decode_observed`，以及 primary/secondary 相同目标只收到一个结构化 Status；该测试不是 `MessageClient -> WebState` mirror 端到端验收，未将其宣称为 WebState 镜像去重证明。
- Qt-only 与 MessageClient 语法证据均通过；最终本轮独立构建命令 `cmake --build C:\JTDX64\build-webui-dev-msys2 --parallel 2` 完成 `[1218/1218]`。构建日志：`C:\JTDX64\deps-webui\build-webui.log`；增量测试重建日志：`C:\JTDX64\deps-webui\build-webui-final.log`。
- 最终 CTest 命令：`C:\msys64\usr\bin\bash.exe -lc 'export PATH=/c/JTDX64/159/bin:/mingw64/bin:/usr/bin:/c/Windows/System32:/c/Windows; ctest --test-dir /c/JTDX64/build-webui-dev-msys2 --output-on-failure'`，结果 `100% tests passed out of 14`；日志：`C:\JTDX64\deps-webui\ctest-webui.log`。未运行 `jtdx.exe`，未连接 CAT/PTT/TX，未做浏览器/HIL/部署。
- 阶段结论：P1 代码、独立配置、完整构建和 14 项 CTest 已通过；P2 TCP/API/SSE、浏览器验证、真实 CAT/PTT/TX/HIL/部署仍未开始。Hamlib SDK 和导入库保留在 `C:\JTDX64\deps-webui` 供下批恢复，不提交大二进制到仓库。
- 结束额度（2026-09-08 收尾快照）：账户共享 5 小时剩余 `26%`、周剩余 `70%`；本批不再开启新批，等待用户继续。

## P0 本批记录（历史记录；不覆盖当前 P1 状态）

### 已完成

- 阅读 `DEVELOPMENT_STATUS_zh-CN.md`、`CMakeLists.txt`、`README`、`main.cpp`、`mainwindow.*`、`MessageClient.*`、`MessageServer.*`、`Configuration.*` 和本地测试块。
- 确认当前仓库实际路径、HEAD、分支和工作树状态。
- 整理用户需求的网络/进程/线程/数据/控制边界、页面、设置、API、状态模型、测试分层和 HIL 禁令。
- 增补端口冲突、鉴权分级、Host/Origin/CSRF、连接限额、SSE 背压/重连、epoch/revision/幂等、过期解码、状态回读和停止优先级等改进项。
- 创建根入口及 `docs/web-ui/` 权威文档，并在 `DEVELOPMENT_STATUS_zh-CN.md` 加索引。
- 文件清单：`WEB_UI_START_HERE_zh-CN.md`、`docs/web-ui/01_需求与边界_zh-CN.md`、`docs/web-ui/02_设计_API_安全_zh-CN.md`、`docs/web-ui/03_阶段验收矩阵_zh-CN.md`、`docs/web-ui/PROGRESS_zh-CN.md`，以及现有状态文档的索引段落。
- 文档级检查：`git diff --check` 通过；未运行配置/编译/CTest/JTDX/浏览器/HIL/部署。

### 未完成

- 没有 `JtdxWebState`、`JtdxWebControl`、`JtdxWebServer` 或 Web 资源代码。
- 没有新增配置 Tab、菜单动作、API 路由、HTTP/SSE 实现或自动化测试。
- 没有配置/编译/运行 JTDX；没有进行浏览器、HIL、生产部署验证。
- 频率、DX、CQ/AutoSeq、停止的最终可复用业务入口和状态回读合同仍待 P1/P4/P5 源码审查。
- P3/P6 的本地浏览器验证仅可在隔离配置、`Rig=None`、已证明不会自动连接硬件且无真实 CAT 连接时进行；真实 CAT/PTT/TX/HIL 仍需单独授权。

### 下一步

P1 先确认 Qt/MinGW/CMake 的独立构建探针，不自动升级依赖；再只做状态模型和数据接入事实/最小实现：确定 `MessageClient`/MainWindow 状态信号的可复用边界、解码字段和新鲜阈值，增加 `JtdxWebState` 及单元测试。不开放控制，不监听 TCP，不新增设置或菜单。

## 恢复命令

```powershell
Set-Location C:\JTDX64\jtdx_sourcecode
git status --short --branch
git log -3 --oneline --decorate
git show --stat --oneline HEAD
rg -n "MessageClient|MessageServer|statusUpdate|status_update|decode|handle_transceiver_update|band_changed|process_Auto|AutoSeq|haltTx|on_stopTxButton_clicked|on_pbCallCQ_clicked" main.cpp mainwindow.cpp mainwindow.h MessageClient.cpp MessageClient.hpp MessageServer.cpp MessageServer.hpp Configuration.cpp Configuration.hpp
```

若恢复时发现 HEAD、分支、工作树或源码锚点改变，先把差异写入本文件，再重新核对，不覆盖他人修改。

## 分阶段交付规则

- 每个阶段一个本地中文 commit，提交前写明修改文件、测试命令、结果和未验证边界。
- 不使用 `git reset --hard`、`git checkout --` 覆盖文件；保留用户已有修改。
- 文档、静态检查、单元、API、浏览器、HIL、生产证据分开记录。
- 没有真实设备验证时，严禁声称已经验证 PTT、实际发射或无线电行为。
- HIL 和向群晖部署都需要独立明确授权。

## 后续批次日志模板

```text
### YYYY-MM-DD 阶段 Px 批次 N
- 开始额度：
- 基线（分支/HEAD/工作树）：
- 本批目标：
- 已完成：
- 未完成/阻塞：
- 修改文件：
- 命令与结果：
- 未运行：JTDX / CAT / PTT / TX / HIL / 部署（按实际填写）
- 下一步：
- 结束额度：
```

### 2026-09-23 阶段 P23：状态紧凑布局与直接操作

- 基线：`main`，初始 `00cba47`，起始工作树干净；仅提交本阶段批准范围。实现提交：`1fcd5830dba4acfd3df091cb00e4c75d9d1488b1`、`70be7d64f67668b6b4b76382bfaaee14d8feb94f`。
- 完成：最近解码与状态/控制响应式布局；周期进度改用 JTDX 校正时钟和周期长度；默认启用且显式禁用可持久化；移除 Web 专属操作 gate，同时保留原生业务安全、未知状态 fail-closed、幂等/epoch/超时与实际回读；修正每次状态快照对在途操作的 reconciliation。
- 验证：完整 Release 构建与 CMake install 退出码 0；最终 CTest 24/24、60.65 秒；`node --check resources/web-ui/app.js` 通过。loopback 隔离夹具在 1280×720 与 390×844 检查布局；手动 `K2XYZ/FN20` 显示匹配回读完成。以上不是 MainWindow/CAT/HIL 证据。
- 包：`C:\JTDX64\deps-webui\release-current\JTDX-2.2.159.2-local-70be7d-P23.zip`；SHA-256 `BCDEC933457F9CEEF706B5FE6A8C4391EDCE15E19202B6950BE3B9260E52BC41`；141 文件，根项 `bin/`、`plugins/`、`share/`、`NOTICE_zh-CN.md`；清洁解压与 staging 文件哈希逐项相同。
- 未运行/未验证：真实 JTDX GUI、CAT/PTT/TX、无线电行为、HIL、LAN/公网访问、公开部署/分发清权。P22 回退包与材料保持不变。完整范围、指标及许可边界见 `P23-电台状态紧凑布局与直接操作_zh-CN.md`。
## 2026-09-20 P4 手动频率表单收尾

- 基线：前端片段基线 `2e378fc`；本轮增加原生手动频率表单、capability 安全状态门、字符串 Hz 请求、POST deadline、响应/SSE 匹配回读和未知结果锁；生产 `MainWindow` gate 保持 `false`。
- 软件验收：隔离 `--serve-browser-frequency` 真实 TCP fixture 提供 `fresh` safe state、`frequency_control_enabled: true` 和模拟 feedback。`14.075000`、`14.076000` 两次请求均 pending 后 completed，request ID 独立；`99999` 服务端返回 `invalid_frequency_hz`，顶部实际频率保持 `14.076000 MHz`。`390` 视口 DOM `clientWidth=scrollWidth=375`，无水平溢出。
- 权威证据：构建退出码 `0`，日志 `C:\JTDX64\deps-webui\p4-frequency-form-final-build.log`；全量 CTest `19/19`、`58.86 sec`，日志 `C:\JTDX64\deps-webui\p4-frequency-form-final-ctest.log`。
- 边界：仅模拟 fixture 软件验证；未启动真实 JTDX，未连接 CAT，未执行 PTT/TX/HIL。异常传输、epoch/token 切换仅静态实现未专项浏览器测；预设频率/频段、生产启用和高层隔离验证仍未完成。详见 `docs/web-ui/P4-FREQUENCY-FORM_验收_zh-CN.md`。
