# JTDX 内置 Web UI：P4-a 普通控制实施契约（DX 选择与 Call 校验）

> 实施更新（2026-09-21）：本契约约束已落地为 `JtdxWebDx` 纯校验、`decode_id` 选择路由、独立 DX generation/source 回读和 MainWindow 最小字段投影。手工来源仍未开放；真实 CAT/PTT/TX/HIL 仍未验证。

任务编号：`JTDX-WEBUI-P4-A-CONTRACT-20260910`
文档性质：源码事实、实施边界、文件清单、测试计划与恢复检查点；本契约批次不实现控制，后续 P4-b 仅落地基础协调器，不改变本契约的生产入口边界。

## 当前结论与基线

- 当前仓库：`C:\JTDX64\jtdx_sourcecode`，分支 `main`，本契约批次基线 `1ef3804`，开始检查时工作树干净；后续 P4-b 从 `5611fcc` 开始，结果以进度日志为准。
- P3 已完成生产 `JtdxWebService` 生命周期、真实 `QAction` 菜单入口及 17 项自动测试；完整 `MainWindow` 窗口构造/人工菜单验收仍未完成，保留到 P6。不能把 P3 的 service/QAction 夹具结果写成完整 MainWindow 验收。
- 本批只完成 P4-a 的普通控制契约准备与源码证据整理：不改生产代码、不改测试、不开放 HTTP control、不启动 JTDX、不连接 CAT/PTT/TX/电台、不做浏览器或 HIL。
- P4-b 已新增未激活的 `JtdxWebControl` 基础协调器及隔离测试；`POST /api/v1/control/*`、真实 DX 应用入口、frequency CAT 适配和真实设备回读证据仍不存在。本文件中的生产入口和完整控制文件清单仍是后续实施约束。
- 配额记录：本批中途快照为五小时剩余 36%、周剩余 25%；末次收尾快照为五小时剩余 30%、周剩余 24%。低于 30% 后只做有界文档、静态检查、精确提交和交接，不开启实现批次或长时间运行。

## 已核对的 DX/Call 事实

| 源码锚点 | 当前行为 | P4-a 可复用性与限制 |
| --- | --- | --- |
| `mainwindow.cpp:806-808`，`doubleClickOnCall()` | 两个 `DisplayText::selectCallsign` 信号连接到真实双击入口；入口从 QTextBrowser 光标和整行文本计算位置后调用 `processMessage()`（`5604-5622`）。 | 这是桌面交互入口，不是 Web 业务接口；不能伪造光标/双击。
| `mainwindow.cpp:5632-5683`，`processMessage()` | 解析整行 `DecodedText`，先可能按消息时间改变 `m_txFirst` 并点击 `TxMinuteButton`（`5664-5672`），再调用 `deCallAndGrid()`。 | 解析结果可作为事实来源，但函数整体不能复用。
| `decodedtext.h:89-94`、`decodedtext.cpp:228-245` | `_callRe`/`_gridRe` 提取并校验 `word2/word3`；`CQ` 特殊格式在 `234-239` 改取呼号；不匹配的 Call/Grid 清空。当前 `_gridRe` 是四位网格表达式。 | 解码来源的 Call/Grid 应复用生产解析结果，带上解码 ID、来源 revision 和新鲜状态；不要在 Web 侧另写正则解析整行。
| `mainwindow.cpp:5696-5711` | `processMessage()` 先用 `Radio::is_callsign(hiscall)` 做呼号门控；不合格时在非 AutoSeq 下还会写报告控件并调用 `genStdMsgs()`。 | Web 选择必须在调用 `Radio::is_callsign` 前先做非空、长度和字符边界；拒绝分支不能调用报告生成或任何控件槽。
| `Radio.cpp:86-94`、`Radio.hpp:42-47` | 当前没有 `CallsignValidator` 类；`Radio::is_callsign()` 是宽松的 `contains` 正则，且在判断长度前对输入调用 `callsign.at(1)`。 | 先 `trimmed().toUpper()`，拒绝空值和长度 `<3`，再调用既有校验；另行拒绝 `CQ/DE/QRZ/RRR/RR73/73` 等协议字段，不能把 `RR73` 当 Call。对既有合法复合呼号保留完整值，匹配时另取 `Radio::base_callsign()`（`Radio.cpp:101-119`），未验证的复合形式暂拒绝并记录待确认，不新增数量限制。
| `mainwindow.cpp:159-160`、`962-975` | `dxCall_alphabet` 只允许 `[A-Za-z0-9/]*`，`dxGrid_alphabet` 供 `QRegularExpressionValidator` 使用；它们不是严格 Call/网格业务校验。 | 不实例化或依赖 QWidget validator；抽取无控件纯校验入口并保留 UI 语义。
| `mainwindow.cpp:6615-6672`，`on_dxCallEntry_textChanged()` | 写入 `m_hisCall` 会取消 AutoSeq 恢复票据、关闭 directed-answer、访问日志簿、填网格、重建 TX 宏模型、更新 `statusChanged()` 和解码窗口。长度 `<3` 时会清理当前 Call。 | 不能从 Web 直接调用此槽；最小提取必须把规范化/校验/字段投影与日志簿、宏模型、恢复票据、控件联动分开，并明确 Web 选择不触发 TX/AutoSeq。
| `mainwindow.cpp:7160-7175`、`6675-6709` | `gridOK()` 只检查前四位 `A-R/A-R/0-9/0-9`，`on_dxGridEntry_textChanged()` 支持 4/6/8/10 位格式化并更新方位/距离。 | 解码 `deCallAndGrid()` 当前仅产出四位 Grid；手工 Grid 若纳入 P4-a，必须以纯函数复用相同边界并拒绝 `RR73`，不能调用 textChanged 槽。
| `mainwindow.cpp:5812-5834`、`6240-6278` | `processMessage()` 改 Call 时会清 QSO 计数/写入控件，更新 Grid/报告并 `genStdMsgs()`；`clearDX()`/`clearDXfields()` 会清字段、重建消息并重置 `CALLING`。 | `select-dx` 只允许明确的 DX 字段投影；不得调用 `processMessage()`、`clearDX()`、`genStdMsgs()`、`QsoHistory::reset_count()` 或改变 QSO 阶段。
| `mainwindow.cpp:5836-5968` | 根据来报内容改 `m_ntx`、`m_QSOProgress`、Tx 单选项和标准消息；`5967` 在 `m_autoTx && !alt` 时点击 Enable Tx，`5968` 的 `alt` 还会 `haltTx()`。 | 这是“选择并准备回复/发射”的完整业务路径；DX 单选必须与开始呼叫分离，任何 `enableTx_mode/click`、`haltTx`、标准消息拼接均禁止。
| `qsohistory.cpp:450-530`、`qsohistory.h:20-57` | `QsoHistory::message()` 更新候选/状态，`reset_count()` 改内部记录；状态枚举包含 `RCQ/RCALL/RREPORT/.../FIN`。 | 选择 DX 不得写 QSO 历史或消费 AutoSeq 候选；若后续开始类控制需要历史状态，另立 P5 契约。

`Radio::is_callsign()` 的宽松匹配还意味着 `is_callsign()` 返回真不能单独证明输入是可选 DX。P4-a 的纯校验应至少包括：长度/字符集边界、协议字段拒绝、基础呼号形状、既有合法复合呼号兼容用例、可选 Grid 的长度和字符边界。未验证的复合形式暂拒绝并记录待确认，不擅自收紧或扩展桌面语义。任何失败均为 `rejected/invalid_call` 或 `rejected/invalid_grid`，不触碰 MainWindow 状态。

## 与 frequency 控制的交界（同批源码复核）

- `mainwindow.cpp:7231-7240` 的 `on_bandComboBox_activated()` 从 `Configuration::frequencies` model 取值后设置 `m_bandEdited`，进入 `band_changed()`；`mainwindow.cpp:7242-7290` 的 `band_changed()` 不是纯 setter：TX/PTT 时可能 `haltTx()`/关闭 Enable Tx，可能清 DX/QSO、刷新 PSK Reporter、启动 monitor，再写 nominal 目标并调用 `setRig()`/`setXIT()`。
- `mainwindow.cpp:7807` 的 `handle_transceiver_update()` 才是实际收发器状态入口；同批复核确认 `observe_rig` 在 `mainwindow.cpp:7895` 附近由 `m_rigState` 更新后投影。目标 nominal/frequency 写入或 HTTP 200 都不能作为 CAT 实际完成证据。
- `LiveFrequencyValidator.cpp:13/47/55` 的 MHz/频段/k 相对 MHz 解析、`validate` 的 Intermediate 语义和 `fixup` 触控件并 emit 的行为，不能直接当作严格安全频段拒绝；P4 实现应抽取纯解析/业务验证，不实例化控件 validator。`FrequencyLineEdit` 不是本入口。
- 因此 `select-dx` 与 `frequency` 必须是两个独立操作；DX 选择不改频率，frequency 控制也不得用全局 revision 或旧频率相等替代专属 CAT 回读。两者共同拒绝 TX/PTT/arming/状态未知，并分别维护自己的业务 generation。

## 与 CAT 恢复及 P5 AutoSeq/CQ 的交叉边界

- P4 DX 只复用 `MainWindow` 已有业务状态以及 AutoSeq/Halt 的既有入口边界；Web 不自行清 DX、重置 CAT/PTT、复制 recovery，也不从 Web 侧直接拼接或启动 AutoSeq/CQ 发射流程。
- CAT `protocol_sync` 必须经过既有 offline/reconnect 流程；只有桌面恢复链确认 PTT 已关闭后，才由桌面恢复链清理旧 DX。Web 只读取并呈现该链路的实际状态，不代行清理或解锁。
- `accepted` 只表示请求已登记，DX 短暂为空也不能宣称通联结束。重连后的新一批完整解码若恢复原 QSO，Web 只呈现实际 QSO/DX 状态，不抢选 DX、不消费恢复候选。
- CAT 交付证据仅作外部基线引用：代码提交 `328cc7a`、交付文档提交 `a89c9da`（artifact code HEAD `9984c38`）。完整 Release CTest `19/19` 含既有 `ftx1_cat_policy_test`，不能写成由本契约或本 Web 文档批次新增；详见 [`DEVELOPMENT_STATUS_zh-CN.md`](../../DEVELOPMENT_STATUS_zh-CN.md) 与 [`RELEASE_NOTES_2.2.159.2.10_zh-CN.md`](../../RELEASE_NOTES_2.2.159.2.10_zh-CN.md)。

## P4-a 控制合同

### 请求和状态门

`select-dx` 请求必须携带有限 ASCII `request_id`、客户端观察到的 `server_epoch` 与 `state_revision`，以及来源之一：

1. `decode_id`：只接受当前有限解码表内、未过期、非离线/回放且 Call 通过生产解析与纯校验的条目；`is_new` 是展示标记，不能剥夺仍有效条目的选择资格。服务端登记后到实际应用前再次按 ID、来源 revision 和当前状态复检。
2. `manual`：携带规范化 Call，可带 Grid/报告；必须走同一纯校验，不接受整行消息或任意 TX 文本。若本批不实施手工来源，必须明确返回 `rejected/manual_source_not_enabled`，不能静默降级为解码选择。

服务端先做 HTTP 鉴权、Host/Origin/CSRF、body/字段大小和 request_id 重复检查，再做 `server_epoch`、revision、应用退出、状态新鲜度和 TX 安全门。P4-a 默认在 `transmitting=true`、PTT=true、`tx_enabled=true`、watchdog/状态不明、业务状态不明或服务正在停止时拒绝；任何 `unknown/stale` 都不能猜测为安全。

应用动作只写入经过校验的 DX Call/Grid（以及明确约定的报告显示值），不能点击控件、调用 `processMessage`/双击槽、启动 CQ/AutoSeq、改变 `m_QSOProgress`/`m_ntx`、写 QsoHistory、改变 TX/RX 频率、拼接标准消息或切换 Enable Tx。原桌面入口继续保留原有语义；若共享 helper，必须由 UI 和 Web 两个调用方分别声明副作用边界。

### accepted 与 completed

结果状态固定为：

`received → rejected`（鉴权、输入、过期 decode、epoch/revision 冲突、退出或安全门失败），或
`received → accepted → pending → completed/failed/timeout`。

`accepted` 只表示已登记并进入主 Qt 事件循环，不表示字段已写入；`completed` 必须由业务状态专属回读证明：规范化 `dx_call`（及请求 Grid/报告，如有）与目标一致、状态 revision 在动作之后变化且来源为 DX 业务观测。当前 `JtdxWebState` 只有全局 `revision`：`observe_status()`（`JtdxWebState.cpp:42-74`）、`observe_rig()`（`76-88`）、`observe_decode()`（`98-132`）都会递增，因此不能仅凭全局 revision 增长证明 DX 应用完成；实现前必须增加 DX 专属 generation/source event，或建立可审计的业务回读标识。

相同目标已本来一致时，返回 `completed` 但结果原因必须是 `already_selected`/幂等 no-op，并附当前状态；不得伪装成收到新的 CAT 或 DX 应用事件。`select-dx` 不应要求 CAT 频率回读作为完成条件，但必须与频率控制隔离，不能用频率 revision 代替 DX 业务回读。

每个 request 的记录至少含 `request_id`、操作名、epoch、初始 revision、目标摘要（不含令牌）、接收/截止时间、状态、结果原因、完成时业务 generation 和有限状态快照；记录数、字段/消息长度、保留时间和 SSE operation 事件大小均设硬上限。相同 `request_id` 且规范化 payload 相同，返回首次结果且不重做副作用；相同 `request_id` 但 payload 不同，返回 `409 request_id_conflict` 且不执行。epoch 改变、服务 shutdown 或超时使旧操作失效。超时不自动重试，迟到回读标记 `unknown/late_feedback` 并附当前快照。

## 冻结文件清单（P4-a 实施时）

| 文件 | 计划变更 | 本批状态 |
| --- | --- | --- |
| `JtdxWebControl.hpp/.cpp` | 新增主线程控制协调器、有限 request 记录、epoch/revision/generation、超时与幂等；不拥有 QWidget，不直接 CAT/PTT。 | P4-b 已实现基础；HTTP、生产入口和完整业务回读未接入 |
| `JtdxWebServer.hpp/.cpp` | 下一实现批次仅新增 `POST /api/v1/control/select-dx` 路由、认证/CSRF/字段上限和控制结果序列化；frequency 完整请求结构、路由和测试契约留下一批；既有 read/SSE/UDP 边界不改。 | 只读服务已存在；本批未改 |
| `mainwindow.h/.cpp` | 抽取最小无副作用 DX 选择/回读入口；保留桌面双击原语义，Web 调用明确禁止 AutoTx/AutoSeq/QSO/TX 副作用。 | 本批未改 |
| `decodedtext.*`、`Radio.*` 或独立纯策略头 | 复用 `deCallAndGrid`/`base_callsign`，补足长度、协议字段、复合呼号和 Grid 边界；不得复制整行解析。 | 仅源码复核；本批未改 |
| `JtdxWebState.hpp/.cpp` | 增加 DX 业务 generation/source event（若由状态层承载），使完成回读可证明来自本次应用。 | 当前仅全局 revision；本批未改 |
| `tests/jtdx_web_control_contract_test.cpp`、必要 `tests/*` | 纯校验、状态门、幂等、epoch/shutdown/超时、回读完成与无 TX 副作用。 | 计划文件；本批未创建 |
| `docs/web-ui/PROGRESS_zh-CN.md`、`03_阶段验收矩阵_zh-CN.md` | 记录本契约、基线/结果提交、P3/P4-a 口径和未验证边界。 | 本批仅同步当前入口与矩阵状态 |

## 测试与验收计划

只在获得实现批次授权且额度允许时执行；测试夹具必须使用隔离 `QSettings`/临时数据目录，禁止附着现用 JTDX。

1. 纯校验：空值、1/2 字符短值（证明不会触发 `Radio::is_callsign` 的 `at(1)` 风险）、大小写/空白规范化、既有合法基础/复合 Call、协议字段、非法字符、未验证复合形式的明确拒绝记录、Grid 4/6/8/10 位和 `RR73`。
2. 解码来源：有效 `decode_id`、过期/不存在/回放/off-air/被淘汰 ID、Call/Grid 与请求摘要不一致、同一 revision 竞态；确认拒绝不会改 DX、QSO 历史、TX 频率或 Enable Tx。
3. 状态门：transmitting/PTT/Enable Tx/watchdog/业务状态未知、应用退出、服务停止、epoch 不一致、revision 冲突；全部返回明确 `rejected` 原因。
4. 应用与完成：首次选择只改变目标 DX 业务观测并在专属 generation 回读后 `completed`；相同目标为 `already_selected`；业务回读不出现则 `timeout`/`unknown`；不得以 HTTP 200、accepted 或全局 revision 代替完成。
5. 幂等与资源：相同 request_id 且相同 payload 不重复应用并返回首次结果；不同 payload 必须 `409 request_id_conflict` 且不覆盖首次结果；控制记录/SSE operation/响应体有界；客户端断开、慢请求和序列化异常只影响当前请求。
6. 频率交界：本批只完成入口审查；frequency 请求结构、专属 CAT generation、文件清单和测试契约留下一批，不能以本文件宣称 frequency 普通控制已冻结。
7. 回归静态/自动：现有 `ui_contract_test`、`jtdx_web_service_test` 和全量 CTest；核对双击原路径的 AutoTx/AutoSeq/周期/QSO 行为未改变。浏览器、完整 MainWindow 人工、CAT/PTT/TX/HIL、部署另行记录，不由 P4-a 自动测试代替。

## 恢复检查点

- 恢复目录：`C:\JTDX64\jtdx_sourcecode`；先执行 `git status --short --branch`、`git log -3 --oneline --decorate`、`git show --stat --oneline HEAD`。
- 本批基线：`main/1ef3804`，工作树干净；预期只新增/同步中文文档，不覆盖用户改动。
- 当前结果：P4-a 仅契约准备，P4 未实现；本批不启动 `jtdx.exe`、不连接 CAT/PTT/TX、电台，不开放 HTTP control。
- 若后续实现前发现 HEAD、工作树、`mainwindow.cpp` 行为或 Web 状态字段改变，先更新本文件的事实锚点和基线，再重新审查；禁止把 `accepted`、HTTP 200、全局 revision 或目标字段预写入当成完成。
- 收尾动作：执行 `git diff --check`；精确查看本批文档 diff 后再提交一个中文本地 commit，不 push、不 amend。提交说明需包含本任务编号、基线、文件、静态检查结果和 P4 未实现/HIL 未验证边界。
