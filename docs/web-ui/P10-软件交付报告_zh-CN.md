# JTDX 内置 Web UI：P10 软件交付报告与简短使用说明

## 1. 结论与范围

本报告对应分支 `main`、P9 提交 `dbe7c5b` 之后的 P10 收尾核对；本报告与实现、测试和文档一起形成新的本地 P10 结果提交，具体提交号以提交后的 `git log -1` 为准。结论分为两层：

- 软件实现和无硬件隔离证据：已完成到当前代码范围。P9 fixture 之外，P10 又用本机测试流量代理补做了错误 `request_id`、错误 `server_epoch` 和确认后重复 POST 的浏览器专项，并对超时/未确认锁补了最小前端保护；未改生产 API、线程、UDP、常驻服务或令牌模型。
- 完整 MainWindow 与设备交付：未完成。自有 `jtdx.exe --test-mode --rig-name p10-mainwindow` 已安全启动并停止；当前 Computer Use 工具没有可操作的原生 MainWindow 面，因此没有把进程启动冒充设置 Tab、菜单、保存重载或窗口视觉验收。真实 CAT/PTT/TX、HIL、部署仍未授权、未执行。

P10 不改变生产功能边界。唯一代码变更是 `resources/web-ui/app.js` 对 `feedback_timeout`/`unconfirmed_feedback` 的前端未知锁保持；AutoSeq 仍表示“启用既有 decode-driven AutoSeq，等待下一批实时解码”，不表示立即呼叫当前 DX 或已经发射。

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
- Timeout 未确认锁专项：最终构建 `C:\JTDX64\deps-webui\p10-timeout-ui-build-20260921-r2.log`；定向 `C:\JTDX64\deps-webui\p10-timeout-ui-focused-20260921-r2.log` 为 `3/3`；全量 `C:\JTDX64\deps-webui\p10-timeout-ui-final-ctest-20260921-r2.log` 为 `22/22`。浏览器中超时行显示 `超时/feedback_timeout`，启动 CQ/AutoSeq 按钮保持禁用、Stop 可用；确认 Stop 后服务端返回 `unconfirmed_feedback`，页面仍保持“服务端保留未确认锁”，未重新开放启动按钮。

## 8. 构建、测试与未完成事项

当前代码提交：以本报告随附的 P10 本地提交为准。P10 只改动 `resources/web-ui/app.js` 的未知锁显示/按钮保护和交付文档；临时代理脚本已删除。

P10 最终权威回归来源为：

- 构建：`C:\JTDX64\deps-webui\p10-timeout-ui-build-20260921-r2.log`，目标 `jtdx_web_server_test` 链接成功。
- 定向：`C:\JTDX64\deps-webui\p10-timeout-ui-focused-20260921-r2.log`，`jtdx_web_server_test` 相关 `3/3` 通过。
- 全量：`C:\JTDX64\deps-webui\p10-timeout-ui-final-ctest-20260921-r2.log`，CTest `22/22`、`100% tests passed`。
- 身份/重复浏览器日志：`C:\JTDX64\deps-webui\p10-proxy-wrong-id.stdout.log`、`p10-proxy-wrong-epoch.stdout.log`、`p10-proxy-duplicate.stdout.log`；其中重复转发日志含 `DUPLICATE_FORWARD`/`DUPLICATE_COMPLETE`。

未完成：完整 MainWindow 原生窗口人工验收、真实 CAT/DX/CQ 回读、PTT/TX、HIL、生产部署和 LAN 启用。下一步由根会话决定是否继续用户可操作桌面验收；不应把本报告的软件隔离证据写成无线电完成证明。
