# P4 频率表单前端切片验收

## 本轮结果（2026-09-20）

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
