# JTDX 内置 Web UI：P6 软件交付报告

## P7 页面与访问收口补充（2026-09-21）

本补充对应后续页面收口批次：Web 服务和前端已移除 token 访问保护，旧 `WebUiTokenSha256` 只安全忽略，不显示、不写回、不进入服务签名。默认 loopback、显式 LAN 地址、Host/Origin/JSON/CSRF 边界、控制默认关闭、页面确认、幂等、超时、业务 generation 和实际回读保持不变。

页面改为自动连接/断线重连；桌面采用左侧 RX/解码/操作、右侧频率/DX/TX/CQ/AutoSeq，窄屏按左后右纵向排列并禁止横向溢出。删去 token 输入和开发合同、epoch/revision 等实现性展示，只保留真实错误、未知/陈旧/断线及“登记不等于完成”的回读提示。

本补充使用无硬件 loopback fixture 和受支持浏览器观察页面；不处理浏览器残留页，不包含真实 `jtdx.exe`、CAT/PTT/TX、HIL、部署或无线电验证。最终构建日志为 `C:\JTDX64\deps-webui\p7-final-build-20260921.log`，全量 CTest 为 `22/22`、`100% tests passed`，日志为 `C:\JTDX64\deps-webui\p7-final-ctest-20260921.log`。

## 范围与结论

本报告对应分支 `main`、基线提交 `c6f14fd` 之后的 P6 有界批次。范围限定为页面二次确认可控性、CQ/AutoSeq/Stop loopback 业务链路、既有 DX 软件合同回归和交付证据整理。结论为“软件隔离部分完成，整体验收未完成”，不代表真实 JTDX、CAT、PTT、TX 或无线电行为已验证。

## API、设置与状态机

- 控制 API 保持现有 `start-cq`、`start-auto-call`、`stop-auto-call`，请求仍必须携带 `request_id`、`server_epoch`、`state_revision` 和 `confirm: true`。
- CQ/AutoSeq 能力仍由独立设置开关控制，默认关闭；未新增端点、UDP 监听、线程、进程、外部服务或前端构建链。
- 启动类请求完成必须匹配 `business_generation` 及 `cq_state`/AutoSeq 回读；HTTP `accepted/pending` 只表示登记或排队。
- Stop 保持高优先级：可在活动 TX/PTT 状态进入既有停止适配；在途启动被安全标记为 `superseded_by_stop`。
- AutoSeq 只启用既有 decode-driven 引擎，等待下一批实时解码，不生成当前 DX 的即时呼叫。

## 调用路径

页面确认 → `POST /api/v1/control/*` → `JtdxWebControl` 的 epoch/revision/安全门 → `MainWindow::dispatchWebBusiness` → 既有 CQ/AutoSeq/停止入口 → `JtdxWebState` 业务 generation 和状态回读。P6 页面确认在 POST 前完成；取消不会生成操作记录。

## 本批变更

- `resources/web-ui/app.js`：页面确认、键盘焦点/取消处理、业务操作名称。
- `resources/web-ui/index.html`、`style.css`：可访问确认对话框及样式。
- `tests/jtdx_web_server_test.cpp`：活动 TX/PTT、Stop 优先、在途启动取消，以及 P6 临时端口夹具。
- `tests/mainwindow_web_frequency_contract_test.cpp`：确认组件、无原生 `globalThis.confirm` 和业务操作名称静态契约。
- `WEB_UI_START_HERE_zh-CN.md`、`docs/web-ui/PROGRESS_zh-CN.md`、本报告、验收矩阵：证据和边界记录。

## 证据

- 构建：`C:\JTDX64\deps-webui\p6-dialog-fixture-build-20260921.log`，受影响目标构建成功。
- 定向静态/契约测试：`C:\JTDX64\deps-webui\p6-dialog-test-fix-focused-20260921.log`，`mainwindow_web_frequency_contract_test` 通过。
- 非 server 全量回归：`21/21`、`100% tests passed`，日志 `C:\JTDX64\deps-webui\p6-dialog-final-no-server-ctest-20260921.log`。
- 浏览器人工：页面内确认框出现，默认焦点为取消；取消后无操作记录；确认后 pending，再由匹配业务回读完成，`cq_armed` 且仅 1 条操作记录。
- 环境污染：`C:\JTDX64\deps-webui\p6-dialog-fixture-focused-20260921.log` 的慢 SSE 失败为残留旧浏览器页连接污染；未修改背压断言，也不把该失败解释为生产源码回归。

## 未验证与下一步

AutoSeq/Stop 的完整浏览器人工链路、重复点击、断线重连、epoch/身份不匹配、timeout 尚未完成；完整 MainWindow 窗口、真实 CAT/DX/CQ 回读、PTT/TX/HIL、生产启用和部署均未验证。下一步应在无旧浏览器客户端的隔离端口完成剩余浏览器场景，再决定是否进行最终全量 CTest；不得把软件回读当作硬件发射证明。
