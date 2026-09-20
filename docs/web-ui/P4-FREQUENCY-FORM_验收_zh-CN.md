# P4 频率表单前端切片验收

## 本轮新增结果（2026-09-21）

### 空候选、上下文变化、失效候选与断开

本轮只扩展隔离 `jtdx_web_server_test` 浏览器夹具，不改变生产 Web 服务：

- `--serve-browser-frequency-empty`：连接后频段和常用频率下拉只有占位项，页面显示“当前模式与地区暂无候选，仍可手动输入”；手动输入 `14.075000` 保留，发送操作仍为 `0` 条。
- `--serve-browser-frequency-invalidated`：初始候选包含 `7.074000`；选择后只填入目标输入、不产生操作结果。随后服务端通过既有 SSE 快照切换为 `FT8 / Region 2 · 2 条候选` 并移除 `7.074000`，预设恢复占位项，目标输入仍保留 `7.074000`。
- 停止本地夹具后，页面显示“未连接/连接断开/保留旧操作快照”，频率发送按钮禁用。浏览器页已关闭，夹具进程已清理。

上述为人工浏览器观察；自动化覆盖仍由状态和候选构建单元测试承担。测试夹具目标重建通过；最终完整 CTest `20/20`、`100% tests passed`、57.18 秒，日志为 `C:\JTDX64\deps-webui\p4-frequency-candidates-browser-boundary-clean-final-ctest.log`。生产 gate 仍为 `false`，未启动真实 JTDX，未连接 CAT/PTT/TX，未做 HIL/部署。

## 本轮新增结果（2026-09-20）

### 频段筛选与常用频率候选

候选由桌面侧 `Configuration::frequencies()` / `FrequencyList_v2` 与 `Bands` 生成，服务端按当前 mode/region 过滤并以有界快照发送；前端不硬编码频率表、不读取 `QComboBox`，也不改变既有 model filter。频段下拉只筛选服务端候选，常用频率下拉的选择只填入现有目标 MHz 输入，不自动发送；手动输入仍保留。

隔离 `--serve-browser-frequency` 夹具中，连接后显示 `FT8 / All · 4 条候选`，可见 `40m`、`20m`；选择 `7.074000 MHz` 后目标输入为 `7.074000`，操作结果保持 `0` 条；切换 `20m` 后只显示 20m 候选，手动输入 `14.075000` 仍可用。`390x844` 窄屏下频率表单纵向布局正常。构建、JavaScript 语法检查和相关定向测试通过；此前 `17/20` 失败由 DLL 搜索顺序混用运行库造成，修正为构建一致的 MSYS2 Qt 优先顺序后，全量 CTest `20/20`、`100% tests passed`、`57.55 sec`，权威日志为 `C:\JTDX64\deps-webui\p4-frequency-candidates-final-ctest-6dbe33d.log`。

本片仍未启动真实 JTDX、未连接 CAT/PTT/TX、未做 HIL 或部署；生产 `MainWindow` frequency gate 仍为 `false`。候选空列表的浏览器切换场景尚未专项人工验证，单元测试已覆盖 mode/region 无匹配结果。

## 本轮结果（2026-09-20）

### 浏览器故障注入与身份锁修复收尾片

基线为 `main/fb16e8b`，生产 `MainWindow` frequency gate 仍为 `false`。本轮使用真实 IAB 与隔离 `--serve-browser-frequency` 做人工故障注入，拦截频率 POST 使其不发送给 fixture。超过 5 秒的传输请求显示未知并锁定发送；同 token 重新连接仍保持锁定；HTTP 200 的 `completed` 且 `confirmed=false`、`confirmed=true` 但 wrong Hz、错误 `request_id`、非法 JSON 均保持未知锁。上述场景顶部实际频率为 `14.074`，`operations` 为零；清除拦截后关闭页面，fixture 已停止。

另复现 HTTP 500 + 匹配 ID/epoch + `completed`/`confirmed=true`/目标频率的矛盾响应：旧实现会在重新连接时错误解锁。`app.js` 已做最小修复，使非 2xx 的矛盾 `completed` 保留请求身份和未知锁，等待匹配回读或新 epoch；合法 `failed`/`rejected`/`timeout` 终态行为不变。重建后的真实 IAB fixture `49153` 复测后，同 token 连接仍为禁用，未知身份锁保留，顶部实际频率为 `14.074`。清除拦截后重载仅作另一用例隔离：`99999` 由服务端真实拒绝为 `invalid_frequency_hz` 且实际频率未改；随后 `14.075` 先 pending、再由匹配回读完成，顶部实际频率为 `14.075`，按钮重新可用。三目标构建退出码为 `0`，日志为 `C:\JTDX64\deps-webui\p4-frequency-fault-final-build.log`；全量 CTest `19/19` 通过、耗时 `58.74 sec`，日志为 `C:\JTDX64\deps-webui\p4-frequency-fault-final-ctest.log`。

本节结果来自浏览器人工故障注入，不是自动 DOM 回归。页面 reload 仅用于用例隔离，不能声称未知锁跨 reload 持久化；`server_epoch` 更换、真实 JTDX/CAT/设备仍未测试，生产 gate 保持关闭。

下批最短路径：先专项覆盖 `server_epoch` 更换、未知结果跨页面与服务端 `unconfirmed_latch` 边界；随后让频段/预设候选来自 `Configuration::frequencies()`、按 region/mode 过滤的 `FrequencyList_v2` 迭代器与 `Bands`，不在 JS 硬编码、不读取 `QComboBox`、不改既有 model filter。生产 frequency gate 继续保持 `false`，直至高层隔离验证完成。

根代理已用隔离 `--serve-browser-frequency` 真实 TCP fixture 完成软件验收。构建退出码为 `0`，日志为 `C:\JTDX64\deps-webui\p4-frequency-form-final-build.log`；全量 CTest 为 `19/19`、`58.86 sec`，日志为 `C:\JTDX64\deps-webui\p4-frequency-form-final-ctest.log`。

浏览器 fixture 提供 `fresh` 安全状态、`frequency_control_enabled: true` 和模拟 feedback。`14.075000` 首次请求先显示 pending 并禁用按钮，随后由匹配回读显示 completed，顶部实际频率为 `14.075000 MHz`；第二次 `14.076000` 同样成功且产生独立 request ID。`99999` 由服务端以 `invalid_frequency_hz` 拒绝，顶部实际频率保持 `14.076000 MHz`。`390` 视口下 DOM `clientWidth=scrollWidth=375`，无水平溢出；本记录不夸大为完整视觉审查。

本轮仅为模拟 fixture 软件验证：未启动真实 JTDX，未连接 CAT，未执行 PTT/TX/HIL；生产 `MainWindow` gate 仍为 `false`。异常传输、epoch 切换和 token 更换已完成静态实现，但未专项浏览器验收。预设频率、频段选择、生产启用、高层隔离验证仍未完成。

## 范围

本切片只增加原生 Web 手动频率表单和安全请求/结果回读。输入为 MHz 正十进制文本，最多 6 位小数；前端按字符串补齐小数位并拼接为整 Hz 字符串，不经过浮点转换。请求只发送到 `POST /api/v1/control/frequency`，带 `request_id`、当前 `server_epoch`、当前安全整数 `state_revision` 和 `frequency_hz`，使用 Bearer/JSON、5 秒 AbortController 截止时间，不自动重试。

发送按钮只在 `frequency_control_enabled === true`、连接及最新快照有效、`online`/`rig_online`/`rig_fresh` 明确为 `true`、`freshness` 为 `fresh`、`tx_enabled`/`transmitting`/`ptt`/`watchdog_timeout` 明确为 `false`，且 epoch/revision 有效、没有自身未终态请求时开放。页面显示具体禁用原因；当前只开放频率能力，DX、CQ、AutoSeq 和发射控制仍未开放。

HTTP `accepted`/`pending` 只显示处理中。只有响应或 SSE `operations` 中匹配自身 `request_id` 与 `server_epoch` 的 `completed`，且 `readback.confirmed === true`、实际 `frequency_hz` 与目标整数字符串完全匹配，才显示完成。失败、拒绝和服务端 timeout 显示原因。响应无法匹配、JSON/传输异常或客户端超时标为结果未知并锁住再次发送，直到同一 epoch 的自身 terminal operation 明确回读或服务 epoch 变化。epoch 变化使旧请求失效并使用新快照；重新连接/更换令牌立即清空旧快照和 SSE 游标，保留未决请求的身份锁以避免跨会话发送，直到匹配回读或新 epoch。

既有只读状态卡片、解码卡片和有限 operations 卡片保持原有有界 DOM 渲染；没有 DX/CQ/预设频率表，没有 localStorage/innerHTML，没有打开 MainWindow 生产 gate。安全随机 ID 使用 `crypto.randomUUID()`，或 `crypto.getRandomValues()` 生成 UUID；无安全随机源时拒绝发送。

## 后续待验证

仍建议后续专项覆盖输入拒绝（空值、零、负号、指数、NaN、超过 6 位小数、超过 19 位 Hz）、completed 未确认/回读不匹配、失败/拒绝/timeout、epoch 切换、传输超时锁定和令牌更换清快照。

本文件不宣称真实 JTDX、CAT、PTT/TX、硬件或 HIL 验证，也不宣称生产频率写控制已开启。
