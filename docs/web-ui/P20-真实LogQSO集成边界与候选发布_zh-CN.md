# P20 真实 LogQSO 集成边界与本地候选交接（2026-09-23）

## 本轮代码与可验证范围

P19 的无当前 DX 请求实际在 `JtdxWebControl::safe_to_dispatch()` 中被拒绝。源代码显示 `MainWindow::m_start2` 初始为 `true`，仅在首次 `handle_transceiver_update()` 后清零；它是启动状态门。此前把它和 `tune/autoTx/iptt` 一起报告为 `tx_path_active` 会把“启动尚未完成”描述成“TX 路径活动”。P20 单独返回 `startup_pending`，原安全拒绝仍生效；真实 AutoTx/Tune/IPTT 路径仍返回 `tx_path_active`。

P20 定向测试调用路径：

- `tests/jtdx_web_control_test.cpp` 直接实例化生产 `JtdxWebControl`，调用 `submit()`；新增断言覆盖 `start2 → startup_pending` 和 `auto_tx → tx_path_active`。既有测试覆盖相同 request ID/payload 返回原结果且不二次 dispatch，也覆盖 QSO confirm 在 draft 关闭且 generation 增长后才通过 `feedback_radio()`。
- `tests/logqso_persistence_test.cpp` 直接构造生产 `LogQSO`，调用 `initWebLogQSO()` 与 `acceptWebQSO()`，在 Qt test data 隔离目录检查真实 ADIF/业务日志追加和不可写失败。修正该测试原有的目录断言：`QDir::isEmpty()` 表示目录内容为空，路径非空应检查 `QDir::path()`。
- `tests/jtdx_web_server_test.cpp` 验证 HTTP 路由到 Control 的请求/回读合同，但它用测试 dispatcher，不调用 MainWindow，也不写 ADIF。

以上测试不是 MainWindow 与 LogQSO 的单次集成验收。MainWindow 的真实路径由 `dispatchWebRadio()` 进入私有草稿函数，再调用其所持有的 `LogQSO`；目前没有可隔离构造该对象的测试入口。构造函数会直接创建 `MessageClient`、`SoundInput/Output`、解码器、配置及音频线程，并绑定生产状态回读/控制对象。当前没有测试构造参数可禁止 UDP/音频副作用并注入合成 DX 上下文。让该构造函数具备注入能力需要跨应用生命周期重构；本轮没有通过放宽安全门、私有内存修改或增加 HTTP/UDP 调试入口绕过这一边界。

## 尚未通过的 Web QSO 集成断言

在 MainWindow 适配与实际 LogQSO 的同一次调用链中，以下项目仍未验证：

- 打开草稿后真实 ADIF/业务日志为零；调用 MainWindow 取消后仍为零。
- 有效合成 DX、时间、频率显式确认后恰好一条 ADIF。
- 相同 `request_id` 重试返回原结果，并且真实 ADIF 记录数不增加。
- 确认期间当前 DX 改变时拒绝；时间/频率上下文变化时拒绝或保持草稿原始上下文，且不误记。
- ADIF 不可写时，Web operation 不完成，草稿/generation 保持未完成，且无成功业务日志；恢复可写目录后的重试结果。

分组件已有的行为不能替代这些集成结论：P19 的 `LogQSO` 测试证明文件追加与写失败原因；`JtdxWebControl` 测试证明 request ID 幂等和回读合同；两者没有共享同一个 Web dispatch 与 LogQSO 调用实例。

## 候选包与验证证据

本地候选目录、ZIP、SHA256、干净解压逐文件哈希、依赖/许可核对、权威构建与 CTest 日志、失败测试记录和回退包索引将在本节完成本机交付整理后写入。候选只表示本地软件包和列明的软件测试结果；不代表人工 UI、CAT/PTT/TX、HIL、LAN 或部署验收，也不创建正式版本/tag。

## 用户操作与回退

候选包内的运行文件仍使用现有 `bin/plugins/share` 根布局；用户应先备份当前 JTDX 数据目录，停止已有程序后，在独立目录解压并按现有 JTDX 启动流程运行。不得覆盖 `C:\JTDX64\159` 安装。若候选出现问题，关闭候选进程，回到现有已验证安装或本索引所列 P18 回退 ZIP；不要把候选数据目录直接复制覆盖到旧版本。

Web UI 无 token。频率、DX、CQ/AutoSeq 和电台面板控制沿用设置页中显式开启的能力开关；默认状态按设置合同。AutoSeq 启动只会启用现有 decode 驱动引擎，必须等待新的有效解码，不能把启动按钮理解为立即呼叫当前 DX。Web QSO 草稿只有显式确认且实际持久化成功后才完成。

真实 MainWindow Web QSO 成功提交、重试无重复持久化、上下文变化行为、正常窗口关闭、CAT/PTT/TX、HIL 和部署仍需后续人工或独立授权验收。
