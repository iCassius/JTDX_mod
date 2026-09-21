# P17：WebUI 功能扩展与本地 RC 交接

## 结论

本阶段在现有 `MessageClient → JtdxWebState → JtdxWebServer → Web UI` 链路上增加了截图对应的 WebUI 菜单、紧凑解码表和受限电台操作面板。未增加第二套 UDP 解码器，未恢复或新增 token 认证，未绕过 JTDX 原有 CAT/PTT/TX 调度。

源码基线为 `2f0c061`；本阶段只保留工作树变更和本地提交，不 push、不 tag。

## 截图到现有业务入口的映射

| WebUI 项目 | JTDX 现有入口 | 回读条件 |
|---|---|---|
| 启用发射 / 终止发射 | `enableTx_mode` / `on_stopTxButton_clicked` | `tx_enabled` 变化；安全门仍检查 CAT、Monitor、PTT、看门狗 |
| 记录通联 | `on_logQSOButton_clicked` | 只打开原生 QSO 记录对话框，不伪造 ADIF 已写入 |
| 清空窗口 / 清除 DX | `on_EraseButton_clicked` / `on_ClearDxButton_clicked` | 状态版本和解码/DX 状态回读 |
| 同步 / 多次解码 / AGC / 窄频 / 解码 | `syncButton`、`on_swlButton_clicked`、`on_AGCcButton_clicked`、`on_filterButton_clicked`、`on_DecodeButton_clicked` | 对应 QWidget 状态或状态版本回读 |
| 生成消息 / CQ | `on_genStdMsgsPushButton_clicked` / 既有 CQ 业务路径 | 消息序列或 CQ 业务状态回读 |
| 跳过 Tx1、Tx1–Tx6 | `on_skipTx1_clicked`、`on_txb1..6_clicked` 与现有编辑校验 | `skip_tx1`、当前 Tx、六条消息回读 |

`AutoSeq` 仍然只做“启用现有解码驱动引擎”；它等待下一批实时解码，不等同于立即呼叫当前 DX，也不代表已经发射。

## API 与安全边界

- 新增受限接口：`POST /api/v1/control/radio`。
- body 只接受 `request_id`、`server_epoch`、`state_revision`、白名单 `action`、`value`、`tx_index`、`text`、`confirm`。
- action 只允许截图对应的有限集合，不接受任意 QWidget、脚本、文件或命令。
- 每个请求经过 Host/Origin/JSON/loopback 或显式 LAN 配置、能力开关、请求幂等、单 pending、超时、epoch 和状态版本检查。
- 启用发射、终止发射、CQ、记录通联在页面要求二次确认；所有结果以主程序实际状态回读为准。
- 旧 token 配置键不读取、不写入、不出现在 WebUI 资源中；保留的是历史审计文档中的迁移说明，不是运行时功能。
- 未确认锁仍 fail-closed；遇到超时或服务重启后的未确认状态，页面提示先确认停止状态或重启 JTDX。

## 验证边界

已完成：

1. `jtdx` Release 增量构建。
2. `jtdx_web_state_test`、`jtdx_web_control_test`、`jtdx_web_server_test`、`configuration_web_ui_test` 通过。
3. 本地浏览器夹具宽屏检查：紧凑解码行、国家字段、选择 DX、受限电台面板、Tx6 当前选择和有界滚动均可见。
4. `node --check resources/web-ui/app.js` 通过。

未完成且不应虚构：

- 没有真实电台、CAT、PTT、音频、发射或 HIL 证明。
- 没有公网/LAN 部署、推送、tag 或正式发布证明。
- `Log QSO` 在 WebUI 中只打开原生记录对话框，是否最终写入 ADIF 仍由桌面用户在原生对话框确认。

## 交接与恢复

保留既有 P14/P15/P16 证据和旧 RC 作为基线；新 RC 必须在最终资源构建后重新打包、校验 SHA256，并使用解压后的直接 `bin/plugins/share` 根结构做启动与资源检查。若电台操作超时或 Web 服务重启，禁止根据 HTTP `accepted/completed` 猜测结果；以当前桌面状态为准，必要时停止发射并重启 JTDX 清除未确认锁。
