# P18 WebUI：QSO 草稿、截图差异与视口验收

日期：2026-09-21；实现提交：`fa7712f`。范围为 `main` 工作树的 Web State/Control/Server、MainWindow Web 适配、LogQSO 复用、内置资源和隔离 fixture。没有启动真实 CAT、PTT、UDP、音频或发射路径；浏览器 fixture 只在内存中模拟状态，不写 ADIF。

最终构建日志：`C:\JTDX64\deps-webui\evidence\history\P18\p18-final-build-fa7712f-20260921.log`；全量 CTest 日志：`C:\JTDX64\deps-webui\evidence\history\P18\p18-final-ctest-fa7712f-20260921.log`，23/23、100%、59.43 秒。对应历史安装/解压暂存目录已在 P20 删除，回退 ZIP 保留在 `C:\JTDX64\deps-webui\history\rollback`。

历史 P18 RC 的安装目录 `p18-install-20260921-fa7712f` 与解压目录 `p18-rc-extract-2.2.159.2.10-fa7712f` 已由 P20 清理。回退 ZIP 位于 `C:\JTDX64\deps-webui\history\rollback\JTDX-2.2.159.2.10-rc-local-fa7712f-p18.zip`。ZIP 直根目录为 `bin/plugins/share`，75 个文件，解压逐文件 SHA256 比对 0 差异；ZIP SHA256：`d3874c04656d68e0312db2c5d7479bb941ae359b844f767901fde5c0d1b08c00`。此前两个 P18 目录均以 `--test-mode` 启动 5 秒后受控停止；这是旧二进制的历史证据，不证明 P20 启动、正常 MainWindow 关闭或设备行为。

## 截图到实现的差异表

| 截图可见项 | P18 页面对应 | 回读/边界 |
| --- | --- | --- |
| 解码行：UTC、SNR、时间差、DF、模式、消息/国家 | `#decodes` 单行网格；国家字段来自状态模型，消息溢出省略 | 最多保留既有上限；列表有界内部滚动，横向隐藏 |
| 启用发射、终止发射 | `radio_enable_tx`、`radio_stop_tx` | 统一安全门；危险动作页面确认；完成必须状态回读 |
| 生成消息、CQ 文本 | `radio_generate`、`radio_cq_text` | 复用现有消息生成入口；CQ 仍需要确认 |
| RRR、跳过 Tx1、同步、多次解码、AGC 补偿、窄频、解码、清空窗口、清除 DX | 对应 radio action | 只允许固定 action 白名单，禁止任意槽函数/API 名称 |
| 记录通联(Q) | `log-qso` 打开 Web 草稿 | 只建立临时草稿，不写 ADIF，不等于完成 |
| QSO 字段编辑、取消、确认提交 | `radio_qso_draft`、`log-qso-cancel`、`log-qso-confirm` | 确认框、字段白名单、必填/时间/频率/当前 DX/重复键校验；确认后才复用 `LogQSO::accept()` |
| Tx1–Tx6 单选和消息编辑 | `select-tx`、`set-tx-message` | 仅 1–6；消息允许可打印 ASCII 空格，拒绝控制字符；完成靠消息/选择回读 |

## API 与参数边界

`POST /api/v1/control/radio` 的 action 固定为：`enable-tx`、`stop-tx`、`log-qso`、`log-qso-confirm`、`log-qso-cancel`、`clear-windows`、`sync`、`multi-decode`、`agc-compensation`、`narrow`、`decode`、`clear-dx`、`generate-message`、`cq`、`skip-tx1`、`select-tx`、`set-tx-message`。请求字段只允许 request/epoch/revision/action/value/tx_index/text/confirm/qso；QSO 只允许 call、grid、mode、report_sent、report_received、name、tx_power、comments、eqsl_comments、start、end、frequency_hz。未知字段、类型、长度、频率范围、危险动作缺少 `confirm=true` 均拒绝。

自动断言覆盖了 QSO 打开必须等待草稿回读、确认必须等待关闭草稿和 generation 增长、同 request ID 幂等、超时/迟到回读、并发/状态 revision 冲突以及未知 QSO 字段拒绝。radio 文本的合法空格回归也由实际 fixture 验证。

## Web Log QSO 完整流程

1. 当前 DX 存在时，`log-qso` 以当前呼号、网格、模式、报告、UTC 起止时间和频率生成默认草稿。
2. 页面字段保持本地编辑态，不会被状态轮询覆盖；取消只清理草稿。
3. 确认按钮先显示页面内二次确认；确认请求携带受限 QSO 对象。
4. MainWindow 校验当前 DX、必填字段、ISO 时间顺序、频率范围和重复键（呼号/开始时间/频率），然后调用现有 `LogQSO` 初始化和 `accept()`；失败返回具体原因。
5. 完成只在草稿关闭、generation 增长的实际状态回读后显示。隔离 fixture 对提交只清理内存草稿并回读，不写入用户日志。

## 浏览器视口证据

使用 `jtdx_web_server_test --serve-browser-automation` 和 Codex In-app Browser 的实际 viewport 覆盖；截图在本批浏览器会话采集。

| 视口 | 页面宽度 | 解码区 | 面板位置/可达性 |
| --- | --- | --- | --- |
| 1280×900 | `innerWidth=1280`，`document/client/body=1265/1265/1265` | 行宽 483px、高 40px；列表 `scrollHeight=clientHeight=40` | 电台面板位于桌面右列；Tx1–Tx6 编辑区可见于面板内部 |
| 390×844 | `innerWidth=390`，`document/client/body=375/375/375` | 行宽 309px、高 29px；列表 `scrollHeight/clientHeight=29`，`overflow-x:hidden` | 电台面板下移到移动页面底部；QSO/Tx 编辑字段按单列/窄屏布局，需页面纵向滚动可达 |

实际操作顺序已验证：打开草稿→编辑姓名/备注且轮询后仍保持→取消并回读关闭→再次打开→编辑并确认二次确认框→提交并回读关闭→逐字编辑 Tx2 为 `WEB TX2 EDIT` 并回读→选择 Tx2 并回读。操作数据均为内存 fixture。

## 菜单、设置和服务生命周期

`mainwindow.ui` 顶层顺序保持 `Language → WebUI → Help`。WebUI enable/open/settings 使用同一个 MainWindow-owned `JtdxWebService`；失败时明确写入错误状态并禁用打开入口。可运行 Qt 测试覆盖禁用、启动、重复打开、同配置重用、停止/重启、在途请求失效、端口占用失败恢复、释放端口、状态/Control 销毁，以及 Configuration 取消、确认、保存重载和非法端口 fail-closed。菜单 XML 顺序、MainWindow 分支、QSO 重复保护由 `mainwindow_web_frequency_contract_test` 断言。

## 未覆盖边界

本批没有原生 MainWindow 人工窗口视觉验收，没有真实 CAT/PTT/TX/无线电回读、HIL、生产 LAN 或部署证据。RC 只可作为本地软件候选；不得把隔离 fixture 的 completed、页面 POST 或 Qt offscreen 测试解释为设备动作或通联已发射。
