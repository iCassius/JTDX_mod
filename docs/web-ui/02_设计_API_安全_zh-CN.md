# JTDX 内置 Web UI：设计、API 与安全契约

## 组件与生命周期

计划新增 `JtdxWebState`、`JtdxWebControl`、`JtdxWebServer` 和原生静态资源。三类 C++ 对象均在 `jtdx.exe` 主 Qt 事件循环中运行；基于 Qt5 Network 的异步 `QTcpServer`，不依赖 Qt WebEngine，不新增前端构建链，不为连接创建线程。现有 `MessageServer` 继续承担既有 UDP 职责，Web 绝不复用其监听器或端口。

启动：读取 Web 专用配置 → 校验地址/端口 → 确认不等于任何 UDP 端口 → TCP `listen()` → 记录实际地址/端口和 `server_epoch`。重复启动返回同一服务。停止：停止接收新连接 → 拒绝控制 → 有界关闭 HTTP/SSE → 取消未确认操作并使旧 epoch 失效 → 关闭监听。应用退出中所有控制返回拒绝。

## 状态和只读 API

`/api/v1/state` 至少含：`application_name`、`application_version`、`instance_id`、`online`、`last_seen`、`frequency`、`band`、`mode`、`dx_call`、`dx_grid`、`report`、`tx_mode`、`tx_enabled`、`transmitting`、`decoding`、`rx_df`、`tx_df`、`de_call`、`de_grid`、`watchdog_timeout`、`sub_mode`、`fast_mode`、`tx_first`、`auto_sequence_state`、`cq_state`、`web_server_state`、`last_status_update`、`last_decode_update`、`recent_decodes`。外层建议加 `schema_version`、`server_epoch`、`state_revision`、`generated_at`、`freshness` 和 `stale_after_ms`。

解码对象至少含短期有效 `decode_id`、时间、SNR、频偏、模式、文本、Call/Grid、是否新解码和来源 revision。队列有硬上限，淘汰最旧项并记计数。状态在事件循环中写入/读取；若未来跨线程必须改为锁或消息转发并增加生命周期测试。

`/healthz` 只描述服务可用性和监听信息；不把 JTDX 业务“在线”伪装为 HTTP 可达。`/api/v1/decodes` 返回有限解码快照。`/api/v1/events` 使用 SSE 时，每个事件带 `id=state_revision` 和有限 JSON；连接/队列/事件大小和空闲时间有上限。断线使用 `Last-Event-ID` 重同步，无法补齐就返回 `resync_required`，不无限保留历史。

## 控制合同

统一流程：`received → rejected`（输入、鉴权、epoch、revision、状态门失败）；或 `received → accepted → pending → completed/failed/timeout`。每条控制记录 `request_id`、操作、epoch、时间、截止时间、初始 revision、目标条件、结果和失败原因，不记录令牌。重复 request id 返回首次结果或 409，不重新执行；服务重启后旧 request 返回 `stale_epoch`。

| API | 必须检查 | 完成条件 |
| --- | --- | --- |
| `frequency` | 现有频率校验、模式/频段、退出和 TX 安全门 | 后续实际状态/CAT 回读匹配目标；写入目标变量不算完成 |
| `select-dx` | 当前有效 `decode_id`/revision、Call/Grid/报告校验 | 状态回读显示目标 DX；不得启动 CQ/AutoSeq，不拼接 TX 消息 |
| `start-cq` | 在线、模式、TX Enabled/AutoSeq 安全门、二次确认 | 状态回读进入 CQ 状态；不表示已经发射 |
| `start-auto-call` | 与 CQ 相同，并检查 AutoSeq 当前允许状态 | 状态回读进入自动流程；不自行调度 TX |
| `stop-auto-call` | 退出/鉴权/幂等检查，停止请求有优先级 | 既有 Halt/AutoSeq 回读为停止/安全；不直接强切 PTT |

开始类请求必须携带客户端看到的 `server_epoch` 和 `state_revision`；服务端登记后仍须在实际调用前重新读取并复检当前 epoch、revision、退出状态和安全门。超时不自动重试；迟到的业务反馈必须标为 `unknown`/迟到反馈并附实际最新状态，不能追溯性地改写为已完成。停止不能被普通队列无限阻塞，但仍不能绕过 TX watchdog 和安全门。控制响应应同时返回状态快照和 `request_id`。

不新增结果查询 API。操作进展通过 SSE 的有限 `operation` 事件和 `/api/v1/state` 中的有限 `operations` 摘要恢复；摘要有数量、大小和保留时间上限，服务重启后旧操作失效，不保留无限日志。

## 鉴权、资源和失败隔离

只读与控制分级鉴权；LAN 必须令牌/强保护，令牌只存摘要或安全配置，不回显、不入日志。控制 POST 做 `Host`/`Origin` 白名单和 CSRF 防护。限制方法、路径、body、头/行、并发/单 IP、慢连接、处理时间、SSE 客户端、响应缓存和日志长度；客户端断开、序列化异常、恶意输入只影响当前请求。Web 异常不得阻塞 JTDX 主流程。

## 真实入口待验证

P1/P4/P5 必须以当前源码复核 `MessageClient` 状态/解码信号、`MainWindow::statusUpdate()`、`MessageClient::status_update()`、`handle_transceiver_update`、`band_changed`/`setRig`、DX 选择、CQ/AutoSeq 和停止入口。已知 UI 路径存在副作用，不能直接把 UI click、双击解码或 `processMessage` 作为 Web 业务 API。若需抽取共用入口，应保持原 UI 语义并增加回归测试。当前没有任何 Web 代码、HTTP 合同或真实设备证据。
