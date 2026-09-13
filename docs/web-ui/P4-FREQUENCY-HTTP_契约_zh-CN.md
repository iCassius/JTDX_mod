# P4 频率 HTTP 最小契约

任务编号：`JTDX-WEBUI-P4-FREQUENCY-HTTP-20260914`。本片从 `main/7c79be7` 开始，目标是把唯一的 `POST /api/v1/control/frequency` 接到现有 `JtdxWebControl`，并用隔离 TCP 夹具验证请求进入生产 dispatcher；不扩展 DX、CQ、AutoSeq，也不改变 UDP/CAT 恢复链。

频率控制必须同时满足三个门：Web 服务启用、设置中的“允许频率控制”显式开关开启、请求通过 Bearer 鉴权和严格同源 Origin。开关默认关闭；只读 Web UI 的既有令牌不会因此获得写权限。关闭时返回 `409 frequency_control_disabled`。配置使用专用 `WebUiFrequencyControlEnabled` 键，开关变化进入服务签名并触发安全重启。

请求体上限 4 KiB，头上限 16 KiB，仍使用 5 秒头超时。拒绝 Transfer-Encoding、Expect、重复 Content-Length、管线请求、畸形 UTF-8 或畸形 JSON。相同 `request_id` 和规范化 payload 返回首次结果并保持幂等；相同 ID 携带不同 payload 返回 `409 request_id_conflict`。JSON 仅接受 `request_id`、`server_epoch`、`state_revision`、`frequency_hz` 四个字段；`frequency_hz` 必须是 ASCII 十进制整 Hz 字符串，随后仍由 `JtdxWebFrequency`/Bands 和生产 MainWindow 适配复核。

成功进入 Control 的响应保留 `request_id`、`operation`、`server_epoch`、`state_revision`、`frequency_hz`、`status`、`reason`、接收/截止/完成时间与当前有限状态快照。`202 accepted/pending` 只代表登记和排队，`completed` 仅由真实频率回读反馈产生；HTTP 200 不代表发射。有限 operations 摘要及其 SSE 回读是后续门槛；本片尚未接入，暂不新增结果查询 API。

隔离测试覆盖分片 body、Content-Length、鉴权、错误 Origin、重复 ID 冲突、非法频率、关闭开关、无 Control 默认拒绝、accepted 不等于 completed、超时/停止重启失效和既有只读回归。测试不启动 JTDX、不连接电台、不执行 PTT/TX/HIL。

当前交付门：有限 `operations` 摘要及其 SSE 更新尚未接入，MainWindow 生产入口因此强制 `enable_frequency_control=false`；本片不宣称用户可用的 Web 频率写控制。只有补齐完成/超时回读并通过独立验收后，才可解除该门。
