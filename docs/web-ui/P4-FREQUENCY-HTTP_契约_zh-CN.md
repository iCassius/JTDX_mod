# P4 频率 HTTP 最小契约

任务编号：`JTDX-WEBUI-P4-FREQUENCY-HTTP-20260914`。本片从 `main/7c79be7` 开始，目标是把唯一的 `POST /api/v1/control/frequency` 接到现有 `JtdxWebControl`，并用隔离 TCP 夹具验证请求进入生产 dispatcher；不扩展 DX、CQ、AutoSeq，也不改变 UDP/CAT 恢复链。

频率控制必须同时满足三个门：Web 服务启用、设置中的“允许频率控制”显式开关开启、请求通过 Bearer 鉴权和严格同源 Origin。开关默认关闭；只读 Web UI 的既有令牌不会因此获得写权限。关闭时返回 `409 frequency_control_disabled`。配置使用专用 `WebUiFrequencyControlEnabled` 键，开关变化进入服务签名并触发安全重启。

请求体上限 4 KiB，头上限 16 KiB，仍使用 5 秒头超时。拒绝 Transfer-Encoding、Expect、重复 Content-Length、管线请求、畸形 UTF-8 或畸形 JSON。相同 `request_id` 和规范化 payload 返回首次结果并保持幂等；相同 ID 携带不同 payload 返回 `409 request_id_conflict`。JSON 仅接受 `request_id`、`server_epoch`、`state_revision`、`frequency_hz` 四个字段；`frequency_hz` 必须是 ASCII 十进制整 Hz 字符串，随后仍由 `JtdxWebFrequency`/Bands 和生产 MainWindow 适配复核。业务拒绝若已解析出 JSON 对象且 `request_id` 是可规范化的可打印 ASCII 字符串，错误 JSON 保留去首尾空白后的可信 ID；缺失或非字符串的 ID 使用新生成的规范化 UUID，并返回 `invalid_control_fields`；字符串不可规范化时使用新 UUID，并返回 `invalid_request_id`。

成功进入 Control 的响应保留 `request_id`、`operation`、`server_epoch`、`state_revision`、`frequency_hz`、`status`、`reason`、接收/截止/完成时间与当前有限状态快照。`202 accepted/pending` 只代表登记和排队，`completed` 仅由真实频率回读反馈产生；HTTP 200 不代表发射。既有 `/api/v1/state` 的 `operations` 数组提供最多 128 条稳定排序的结果摘要，每条含时间字段和 `readback.confirmed`、generation、有限频率/DX 回读；摘要不嵌套 `current_state`，也不包含凭据。

SSE `snapshot` 同样携带 `operations`。服务以 Control 的独立 operation revision 检测仅操作状态的变化，并使 snapshot event id 变化；event id 还绑定当前服务 epoch 和 Control 替换代次，不能只依赖 State 全局 revision。历史不保留，任何 `Last-Event-ID` 都走有界 `resync_required` 加完整 snapshot；服务重启按新 epoch 过滤旧结果。timeout 可保留最后可信快照，但 `readback.confirmed=false`，不能解释为当前实际频率。

隔离测试覆盖分片 body、Content-Length、鉴权、错误 Origin、重复 ID 冲突、非法频率、关闭开关、无 Control 默认拒绝、accepted 不等于 completed、state GET/SSE 的 pending→completed 与 timeout 回读、event id 变化、Last-Event-ID 重连、epoch 过滤、停止重启失效和既有只读回归。测试不启动 JTDX、不连接电台、不执行 PTT/TX/HIL。

当前交付门：有限 `operations` 摘要及其 SSE 更新、MainWindow 频率适配隔离契约和显式配置开关已接入并通过软件隔离验收；默认配置仍关闭，真实 CAT 回读、完整窗口验收与 HIL 仍未交付。本片不把软件隔离结果宣称为真实设备频率控制完成。
