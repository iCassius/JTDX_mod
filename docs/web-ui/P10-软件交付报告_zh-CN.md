# JTDX 内置 Web UI：P10 软件交付报告与简短使用说明

## 1. 结论与范围

本报告对应分支 `main`、P9 提交 `dbe7c5b` 之后的 P10 收尾核对；本报告与实现、测试和文档一起形成新的本地 P10 结果提交，具体提交号以提交后的 `git log -1` 为准。结论分为两层：

- 软件实现和无硬件隔离证据：已完成到当前代码范围。P9 fixture 之外，P10 又用本机测试流量代理补做了错误 `request_id`、错误 `server_epoch` 和确认后重复 POST 的浏览器专项，并对超时/未确认锁补了最小前端保护；未改生产 API、线程、UDP、常驻服务或令牌模型。
- 完整 MainWindow 与设备交付：未完成。自有 `jtdx.exe --test-mode --rig-name p10-mainwindow` 已安全启动并停止；当前 Computer Use 工具没有可操作的原生 MainWindow 面，因此没有把进程启动冒充设置 Tab、菜单、保存重载或窗口视觉验收。真实 CAT/PTT/TX、HIL、部署仍未授权、未执行。

P10 不改变生产功能边界。唯一代码变更是 `resources/web-ui/app.js` 对 `feedback_timeout`/`unconfirmed_feedback` 的前端未知锁保持；AutoSeq 仍表示“启用既有 decode-driven AutoSeq，等待下一批实时解码”，不表示立即呼叫当前 DX 或已经发射。

## P12 追加：本地控制诊断日志

P12 将控制生命周期接入现有 MainWindow recovery log：`jtdx_recovery.log` 位于实例已有可写数据目录，主动文件上限 `256 KiB`，只保留一个 `.1` 轮转文件。P13 又把 Hamlib CAT 诊断的同文件写入统一到同一组件，并保持既有本地 ISO 毫秒时间格式、UTF-8 和 `key=value` 字段分隔。写失败、打开失败或轮转失败只影响诊断，不影响控制、安全门、回读或未知锁；日志不是恢复状态源，也不参与新 epoch 或多浏览器解锁。

每条 Web 记录包含本地 ISO 毫秒时间、规范化 `request_id`、操作、事件（`reject`/`accepted`/`transition`/`duplicate`）、状态、限长原因、generation/state revision 和有限 readback 摘要。Web 字段值的换行/制表符、反斜杠和等号会被清洗或转义并限长；通用写入器保留既有 `key=value` 分隔。日志不写 Web Authorization、token、密码、原始 HTTP、call/grid 或其他凭据。相同 request ID 与相同 payload 的重复请求只记录 `duplicate`，不会再次执行 dispatcher。

P12 自动证据：构建 `C:\JTDX64\deps-webui\p12-build-20260921-r3.log`；定向配置、Control、MainWindow 合同、Server、Service 与本地日志测试为 `6/6`，日志 `C:\JTDX64\deps-webui\p12-focused-20260921-r2.log`；全量 CTest 为 `23/23`、`100% tests passed`、57.29 秒，日志 `C:\JTDX64\deps-webui\p12-final-ctest-20260921.log`。P13 代码来源为 `d6dbad6`、`a19146f`、`dc62904`；统一日志和受影响目标构建为 `C:\JTDX64\deps-webui\p13-final-build-20260921.log`，定向测试为 `C:\JTDX64\deps-webui\p13-focused-20260921.log`，页面资源构建为 `C:\JTDX64\deps-webui\p13-ui-build-20260921.log`，最终全量 CTest 为 `23/23`、`100% tests passed`、57.48 秒，日志 `C:\JTDX64\deps-webui\p13-final-ctest-dc62904-20260921.log`；`node --check resources/web-ui/app.js` 通过。新增边界测试覆盖临时目录、Unicode/换行注入、key=value 兼容、超长单条、超大既有文件轮转、轮转失败、重复生命周期和不可写路径；真实 MainWindow、CAT/PTT/TX/HIL、部署和 LAN 仍未验证/未授权。

## P13 最终软件交付摘要与人工验收清单

最终代码来源为 `d6dbad6`（统一恢复日志写入边界）、`a19146f`（未确认锁恢复提示）和 `dc62904`（轮转失败边界测试）；最终文档提交另列，不把文档提交号写入自身。统一日志主程序/测试目标按 `p13-final-build-20260921.log` 构建，页面资源按 `p13-ui-build-20260921.log` 重建；唯一权威全量测试日志为 `p13-final-ctest-dc62904-20260921.log`，共 `23/23`、`100%` 通过。P13 发现并修复了 Hamlib 与 MainWindow 分别直接写同一日志文件的问题，以及超大既有文件轮转后 `.1` 仍可能超限的问题；未改变 CAT 诊断字段顺序、时间格式、UTF-8 或 `key=value` 解析约定。

当前默认边界：Web UI 关闭、自动端口选择开启、默认绑定 `127.0.0.1`、LAN 关闭；频率、DX、CQ/AutoSeq 控制分别关闭；无 token 输入、校验或写回。显式 LAN 仍要求用户主动开启并配置精确 allowed origin。控制完成必须有实际频率、DX 或业务状态回读；`accepted/pending`、HTTP 200、日志写入和 UDP datagram 都不等于完成。AutoSeq 只启用既有 decode-driven 流程并等待下一批实时解码，不等于立即呼叫当前 DX。

人工验收清单（只使用用户自有实例和授权环境）：

1. 在设置中核对 Web UI 默认关闭、loopback、自动/手动端口，确认保存/重载、端口冲突、停止/启动 Web 服务和菜单重复操作；不启用 LAN，除非用户明确授权。
2. 核对页面无 token 输入；分别开启频率/DX/CQ/AutoSeq 后，确认登记、处理中、匹配回读和拒绝结果，且未回读时不显示完成。
3. 在 AutoSeq 场景确认页面文案是“等待实时解码”，不是立即呼叫当前 DX；确认 Stop 优先级、`idle/disabled` 回读和未知结果保护。
4. 若已 dispatch 的请求超时并保留 `unconfirmed_feedback`，先执行安全 Stop；若普通控制仍锁定，关闭并重启 JTDX 以重建 `JtdxWebControl`。刷新网页、重启 Web 服务、读取/删除日志均不能解锁，也不得自动重发。
5. 仅在单独授权后验证真实 CAT/DX/CQ 回读、PTT/TX/HIL、长时运行、部署和 LAN；本批没有执行这些操作，也没有修改 UDP 监听/配置、线程或服务架构。

## 2. 原交付要求追踪

下表把设计基线的 14 项交付要求归并呈现；原始编号与完整边界见 [`01_需求与边界_zh-CN.md`](01_需求与边界_zh-CN.md)。R15 作为授权、恢复和交付边界另列，不把它误写成设备验收。

| 项目 | 原始编号 | 当前结论与证据 |
| --- | --- | --- |
| 1. 目标界面与原生资源 | R01 | 已有原生 HTML/CSS/JavaScript、Qt Resource、状态/RX/DX/TX/频率/CQ/AutoSeq 分区；P8 已有 1280 与 390 视口证据；完整 MainWindow 视觉未验。 |
| 2. 独立 TCP 与默认 loopback | R02 | 已实现独立 Web TCP 端口、自动/手动端口、默认 loopback；P2/P3/P9 loopback 证据通过。 |
| 3. UDP 完全隔离 | R03 | Web 路径不新增 UDP listener、不改 UDP 配置、不启动 message_aggregator；静态与测试证据通过，未做生产部署观察。 |
| 4. 线程/进程/服务边界 | R04 | Web 复用主 Qt 事件循环；既有 `jtdxjt9` 解码子进程仍存在，不计为 Web 新增；无 Web 常驻线程/外部服务。 |
| 5. 复用内部状态流 | R05 | `MessageClient` 状态/解码信号进入 `JtdxWebState`；不重复解析 UDP、不从控件反读完整状态。 |
| 6. 有界状态与解码 | R06 | 状态 revision、freshness、server epoch、有限解码和 SSE 重连/重同步已有测试；P9 陈旧 UI 锁定已观察。 |
| 7. 频率与 DX 分离控制 | R07 | 频率安全门、实际回读、DX 独立 generation/source/decode_id、过期解码 UI 隐藏与服务端拒绝均已有软件证据；未做 CAT 回读。 |
| 8. CQ/AutoSeq/Stop 安全语义 | R08 | 页面确认、Stop 优先、在途启动 `superseded_by_stop`、业务 generation/readback、timeout/unknown latch 已有软件与浏览器证据；无真实 TX。 |
| 9. 最小 API 路由 | R09 | 只提供读 API、SSE 和 frequency/select-dx/start-cq/start-auto-call/stop-auto-call 控制 POST；没有 Reply、FreeText、音频或通用远程控制。 |
| 10. 结果、时间、原因与幂等 | R10 | `request_id/server_epoch/state_revision`、时间、deadline、status、reason、readback 已进入结果；P10 浏览器专项验证错误身份保护和确认后重复 POST 幂等；HTTP accepted/pending 不当完成。 |
| 11. 设置、持久化与菜单 | R11 | Web 专用配置键、设置入口、重启和打开 UI 代码路径已有；自有 MainWindow 因当前工具不能操作原生窗口，保存重载/菜单重复打开仍未人工验收。 |
| 12. 状态、解码与事件推送 | R12 | `/api/v1/state`、`/api/v1/decodes`、SSE、断线重连、epoch/revision 重同步已有软件证据；完整 MainWindow 实例状态到页面的人工链路未验。 |
| 13. 安全与资源边界 | R13 | 当前用户需求为无 token；仍保留显式 LAN、Host/Origin、JSON/CSRF、body/header/连接/SSE 限制；没有恢复 token。 |
| 14. 测试与交付分层 | R14 | 静态、单元、API、loopback 浏览器和恢复文档已分层；HIL、生产部署和完整窗口人工验收明确列为未完成。 |
| 授权/恢复/设备边界 | R15 | 不启动真实电台、不执行 CAT/PTT/TX/HIL/部署、不替用户开启 LAN；真实设备证据须另行授权。 |

## 3. API、请求与响应口径

只读路由：`GET /`、`GET /healthz`、`GET /api/v1/state`、`GET /api/v1/decodes`、`GET /api/v1/events`。

控制路由：

- `POST /api/v1/control/frequency`：`request_id`、`server_epoch`、安全整数 `state_revision`、字符串 `frequency_hz`。
- `POST /api/v1/control/select-dx`：`request_id`、`server_epoch`、`state_revision`、当前 `decode_id`；服务端重新查验新鲜解码和 Call/Grid。
- `POST /api/v1/control/start-cq`、`start-auto-call`、`stop-auto-call`：`request_id`、`server_epoch`、`state_revision`、`confirm: true`。

所有控制响应保留 `request_id`、`operation`、`server_epoch`、`status`、`reason`、`received_ms`、`deadline_ms`、`completed_ms`、初始 revision、`generation` 和有限 `readback`。`accepted/pending` 只表示登记/排队；`completed` 必须由匹配的业务、DX 或实际频率回读确认。服务重启产生新 epoch，旧请求不自动重试；已 dispatch 但回读未知时，服务端保持 `unconfirmed_feedback` 锁。

## 4. 配置与业务调用路径

Web 专用持久键为：

`WebUiEnabled`、`WebUiAutomaticPort`、`WebUiPort`、`WebUiBindAddress`、`WebUiAllowLan`、`WebUiAllowedOrigin`、`WebUiFrequencyControlEnabled`、`WebUiDxControlEnabled`、`WebUiAutomationControlEnabled`。

默认服务关闭、默认 loopback、控制能力分别关闭；无 token 输入、无 token 校验、无 token 写回。配置/菜单路径为 `Configuration` → `JtdxWebService` → `JtdxWebServer`；运行状态/解码由 `MessageClient` 信号进入 `JtdxWebState`。控制路径分别为：

`页面确认 → HTTP POST → JtdxWebControl epoch/revision/安全门 → MainWindow 最小 dispatch → 既有业务入口 → State generation/readback → SSE/state operations`。

DX 只更新 DX 输入投影，不进入双击、QSO、标准消息、AutoSeq、Enable TX、PTT 或 TX 路径。Stop 复用既有停止优先级。AutoSeq 只等待实时解码，不凭空生成目标。

## 5. 简短使用说明

1. 在桌面设置中显式开启 Web UI；优先保持 `127.0.0.1` 和自动 TCP 端口。LAN 绑定只有在用户明确需要时才开启，并填写匹配的 allowed origin。
2. 浏览器打开设置页显示的地址，先确认顶部为已连接/新鲜，再查看状态、解码、DX、TX 和操作结果。
3. 频率、DX、CQ/AutoSeq 控制还需要分别开启；页面确认后只能看到登记/处理中，必须等待匹配回读才算完成。
4. “启用 AutoSeq”只表示启用既有自动序列并等待实时解码；“选择 DX”不等于开始呼叫。Stop 在活动或在途流程中优先使用。
5. 若页面提示结果未知、陈旧或断线，不重复发送同一动作；先等待明确回读或服务新状态。timeout 后若是已 dispatch 的不确定操作，服务端可能继续保持 `unconfirmed_feedback` 锁，这是安全边界。

## 6. 无硬件 MainWindow 隔离启动审查

代码事实：`main.cpp` 支持 `--test-mode` 和唯一 `--rig-name`，测试模式使用独立 Qt 测试路径/应用名并形成独立 lock key；`Configuration` 默认 `Rig=None`、`AcceptUDPRequests=false`、Web UI 关闭。`MainWindow` 保留既有音频线程和 `jtdxjt9` 解码子进程，Web 不新增或删除它；初始 CAT 打开经过延迟 `rigOpen()`，不是 Web 控制动作。

本批仅启动并停止自有实例：

```text
PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;C:\JTDX64\159\bin;...
C:\JTDX64\build-webui-dev-msys2\jtdx.exe --test-mode --rig-name p10-mainwindow
```

观察到自有 `jtdx.exe` 与其既有 `jtdxjt9.exe` 子进程，随后均已停止；没有终止用户实例，没有连接真实 CAT，没有执行 PTT/TX。当前工具没有原生 MainWindow 的可操作窗口面，因此以下仍需用户在桌面上手动完成：Web 设置 Tab 默认值、保存重载、自动/手动端口、端口冲突失败、启停重启、菜单重复打开、真实状态到 Web 只读路径和退出释放。

## 7. P10 浏览器专项证据

P10 使用临时本机 TCP 代理把浏览器流量转发到自有 `--serve-browser-automation-p9` fixture；代理只改 HTTP 测试流量，不注入 fixture 内存、不绕过确认。临时代理脚本已删除，fixture、代理和浏览器页已清理。

- 错误 `request_id`：真实点击“启用 AutoSeq”、完成页面确认后，回包只改 `request_id`；页面显示“响应无法与本次命令安全匹配，结果未知”，操作保持 `处理中/awaiting_feedback`，启动按钮锁定，Stop 保留。
- 错误 `server_epoch`：同样的确认后真实 POST，回包只改 `server_epoch`；页面保持未知/处理中保护，不把 HTTP 回包误当完成。
- 确认后重复 POST：代理把同一已确认 `start-auto-call` POST 实际转发两次；页面只保留 1 条 `启用 AutoSeq/处理中/awaiting_feedback`。代理日志 `C:\JTDX64\deps-webui\p10-proxy-duplicate.stdout.log` 含 `DUPLICATE_FORWARD` 与 `DUPLICATE_COMPLETE`。
- P9 既有专项：新鲜 DX 选择、陈旧隐藏、在途 Stop、取消/重复点击零 POST、跨 epoch、timeout 的证据见 [`PROGRESS_zh-CN.md`](PROGRESS_zh-CN.md) 和 `p9-*` 日志。
- P10 原始 Timeout 观察：最终构建 `C:\JTDX64\deps-webui\p10-timeout-ui-build-20260921-r2.log`；定向 `C:\JTDX64\deps-webui\p10-timeout-ui-focused-20260921-r2.log` 为 `3/3`；全量 `C:\JTDX64\deps-webui\p10-timeout-ui-final-ctest-20260921-r2.log` 为 `22/22`。当时页面显示 `超时/feedback_timeout`，启动 CQ/AutoSeq 禁用、Stop 可用；P11 已修正服务端 Stop 误拦截和前端完成后误解锁，当前权威结论见下节。

## 9. P11 追加核查与当前交付边界

### 9.1 主程序资源链

`CMakeLists.txt` 的 `resources/web-ui/app.js` 通过 `add_resources` 进入主程序 `wsjtx_RESOURCES_RCC`，`jtdx` 目标包含生成的 RCC。`C:\JTDX64\deps-webui\p11-build-20260921-r3.log` 明确记录 `qrc_jtdx.cpp` 重新生成并最终链接 `jtdx.exe`；`p11-build-20260921-r5.log` 记录更新 fixture 资源后 `jtdx_web_server_test.exe` 最终链接。P11 因此同时核实了主程序资源目标和测试资源目标，没有把 fixture 资源当作主程序交付证明。

### 9.2 未确认锁后的 Stop 语义

`JtdxWebControl::submit()` 现在只对 `unconfirmed_latch_` 放行 `StopAutoCall`；其他新启动/频率/DX改变操作仍返回 `unconfirmed_feedback`。Stop 仍经过 epoch/revision、pending/一次性 begin 和现有业务回读。`MainWindow::applyWebStopAutoCall()` 的顺序仍是既有 `on_stopTxButton_clicked()` 后 `on_AutoSeqButton_clicked(false)`，没有新增 PTT/TX 直写。旧超时回调因 request/pending/epoch 校验不能复活，Stop 完成也不清除旧 latch。

### 9.3 最终验证

- 构建：`p11-build-20260921-r3.log`（主程序 `qrc_jtdx.cpp`、`jtdx.exe` 与受影响测试目标）及 `p11-build-20260921-r5.log`（最终 Server fixture 链接）。
- 定向：`C:\JTDX64\deps-webui\p11-pre-final-focused-20260921.log`，`configuration_web_ui_test`、Control、MainWindow 合同、Server、Service 为 `5/5`。
- 全量：`C:\JTDX64\deps-webui\p11-final-ctest-20260921.log`，CTest `22/22`、`100% tests passed`、58.38 秒。
- 浏览器：更新 timeout fixture 实际先显示 `AutoSeq enabled`/`armed/calling`，超时后启动按钮禁用、Stop 可用；确认 Stop 后操作卡为 `已完成（已确认）/automation_stopped`，回读为 `idle/idle` 与 AutoSeq `disabled`，启动按钮仍禁用；页面状态说明 Stop 完成不解除旧未确认锁。

### 9.4 MainWindow 集成边界与人工步骤

已有 `configuration_web_ui_test` 真实运行 Configuration 对话框，覆盖取消不落盘、确认保存、重载、默认 `Rig=None`/loopback/控制关闭、无 token 编辑器和非法端口 fail-closed。完整 MainWindow 没有可安全复用的测试构造/注入层；直接创建它会启动既有音频线程、解码子进程、QSettings 和 CAT 初始化，所以不以源码字符串或重复进程启动冒充人工窗口验收。

用户后续若授权桌面手工验收，可在独立实例中按以下步骤执行：

1. 用 `jtdx.exe --test-mode --rig-name <唯一名>` 启动，确认默认 Web 关闭、`Rig=None`、UDP 控制关闭。
2. 设置 → Web UI：开启 Web、保持 `127.0.0.1`、自动端口；保存后确认状态为运行中且地址可打开。
3. 重开设置确认值保持；改手动端口后保存，再次重开确认；重复点击菜单只保持一个服务/一个地址。
4. 用另一个本机 TCP 监听占住手动端口，确认页面/设置显示失败且不改 UDP；切回自动端口后确认服务恢复。
5. 停止/重启服务和关闭窗口，确认监听释放；全程不连接 CAT、不启用 LAN、不执行 PTT/TX。

### 9.5 诊断与分层结论

Web 未新增远程日志 API；本地 `jtdx_recovery.log` 由现有 MainWindow/Hamlib 写入路径统一受限，并加入 Web 控制诊断摘要，不作为状态源或 Web 访问审计日志。`/api/v1/state` 与 SSE 的有界 `operations` 已实际实现并回归 `request_id`、`received_ms`/`deadline_ms`/`completed_ms`、`reason`、generation 和 `readback.confirmed`；软件实现、自动 CTest、loopback 浏览器已验证；原生 MainWindow 人工操作、真实 CAT/DX/CQ 回读、PTT/TX、HIL、部署和 LAN 启用仍未验证/未授权。

## 8. 构建、测试与未完成事项

当前代码提交：以本报告随附的 P10 本地提交为准。P10 只改动 `resources/web-ui/app.js` 的未知锁显示/按钮保护和交付文档；临时代理脚本已删除。

P10 最终权威回归来源为：

- 构建：`C:\JTDX64\deps-webui\p10-timeout-ui-build-20260921-r2.log`，目标 `jtdx_web_server_test` 链接成功。
- 定向：`C:\JTDX64\deps-webui\p10-timeout-ui-focused-20260921-r2.log`，`jtdx_web_server_test` 相关 `3/3` 通过。
- 全量：`C:\JTDX64\deps-webui\p10-timeout-ui-final-ctest-20260921-r2.log`，CTest `22/22`、`100% tests passed`。
- 身份/重复浏览器日志：`C:\JTDX64\deps-webui\p10-proxy-wrong-id.stdout.log`、`p10-proxy-wrong-epoch.stdout.log`、`p10-proxy-duplicate.stdout.log`；其中重复转发日志含 `DUPLICATE_FORWARD`/`DUPLICATE_COMPLETE`。

未完成：完整 MainWindow 原生窗口人工验收、真实 CAT/DX/CQ 回读、PTT/TX、HIL、生产部署和 LAN 启用。下一步由根会话决定是否继续用户可操作桌面验收；不应把本报告的软件隔离证据写成无线电完成证明。
