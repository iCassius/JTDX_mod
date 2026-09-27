# 2026-09-27 PI4DX 通联与 CAT 故障诊断

## 结论摘要

截图和本机日志对应一次 AutoSeq 对 PI4DX 的 FT8 发射。JTDX 在 2026-09-27 11:03:30.474 UTC 记录向 BI7KGD 发送 PM01；紧接着 FTX-1 CAT 协议同步轮询于 11:03:31.775 UTC 以 EPROTO 硬失败，故障记录中的发射意图、PTT 实际状态和 PTT 已知位均为 true。当前日志没有证明硬失败已到达 MainWindow 的 CAT 恢复处理，也没有 MainWindow 重连、PTT-off 回读或 DX 释放的后续记录。

之后可见的 RC2SD PI4DX R+08 和 RC2SD PI4DX 73 是发给 PI4DX 的消息，不是对 BI7KGD 的应答。单次 PI4DX 发射之后没有观察到 BI7KGD 对 PI4DX 的回复。现有证据支持“CAT 硬故障可能打断了后续 AutoSeq”，但尚不能确定失败通知在哪一层中断，也不能把配置窗口可见、嵌套模态对话框或 Qt 重入认定为根因。

当前不建议仅凭这张截图提高被叫优先级。优先级改变的是候选竞争；本次首先需要确认故障信号是否穿过 Configuration 到达 MainWindow，以及重连时是否有在线 PTT-off 回读。没有确认之前，不能把候选优先级当作 CAT 故障恢复的替代修复。

## 证据范围与时间

- 截图：C:\Users\cassi\AppData\Local\Temp\codex-clipboard-513659b8-7f56-434f-892a-d329978dd7c5.png。画面显示 JTDX 2.2.159.029、构建时间 2026-09-24 15:12:04 UTC、选中 PI4DX、21.074 MHz、RX 1776/TX 2109。右侧解码表包含 PI4DX 与 RC2SD 的消息。画面时钟标为 UTC、日期为 2026-09-27。
- 静态运行文件：C:\JTDX64\159\bin\jtdx.exe，文件版本 2.2.159.29，产品版本 2.2.159.29 ec26de，SHA-256 0B680FD996216BFADE75707B1CE5067CE171121295853B708056D39FF0DC08B2。本次只读取文件元数据和哈希，没有启动该文件。
- 本批开始时的源码仓库基线：C:\JTDX64\jtdx_sourcecode，分支 main，HEAD e001833。该基线的 AutoSeq 业务逻辑与 P029 对应提交 ec26de6 一致；之后的 P030 提交只涉及 Web、网络、配置、版本和测试范围。本批初始诊断提交为 63527f7；本文也记录了根复核后补上的观测准确性与异常隔离修正。
- 本机 JTDX.ini 当前最后写入时间为 2026-09-28 00:00:45（本地时间），晚于事件。因此当前配置不作为 9 月 27 日事件时的设置证据，也不据此推断当时 AutoSeq、完成标记或调试开关。
- 2026-09-28 的进程快照未发现 jtdx.exe。这是观察时的快照；没有证据说明进程何时或为何退出，本次没有操作运行进程。
- 原始大日志没有复制到源码仓库。下列引用只给出文件、行号和必要字段。

## 通联与故障时间线

时间均为 UTC；恢复日志的原时间戳为 +08:00。

| 时间 | 证据 | 可支持的结论 |
|---|---|---|
| 11:03:15 | 202609_ALL.TXT:191615，CQ PI4DX JO21 | PI4DX 发出 CQ。 |
| 11:03:30.474 | 202609_ALL.TXT:191616，21.074 MHz、TX +2109 Hz、PI4DX BI7KGD PM01 | 唯一观察到的 PI4DX→BI7KGD 发射。 |
| 11:03:30.378–11:03:30.700 | jtdx_recovery.log:841-843 | AutoSeq 记录 selected=PI4DX、plannedTxFirst=true；随后请求 FT8 PTT，记录 txFreq=2109、rxFreq=2277，并在 250 ms 后确认 PTT。 |
| 11:03:31.775 | jtdx_recovery.log:847 | FTX-1 protocol_sync 返回 EPROTO，ptt_intent=true、ptt_actual=true、ptt_known=true、safe_idle=false、decision=hard-failure。 |
| 11:05:15、11:05:45、11:08:15、11:11:45 | 202609_ALL.TXT:191618-191619, 191622, 191625 | PI4DX 后续 CQ 可继续被解码记录。 |
| 11:06:15、11:06:45 | 202609_ALL.TXT:191620-191621 | RC2SD PI4DX R+08 与 RC2SD PI4DX 73；它们针对 PI4DX，而不是针对 BI7KGD。 |
| 15:11:15 | 202609_ALL.TXT 文件末行 | 当日 ALL 解码日志继续到 15:11:15；恢复日志仍停在 11:03:31.775 的 CAT 硬失败。 |

202609_ALL.TXT:191616 是该观察窗口里唯一的 PI4DX 发射记录。日志里没有发现随后由 BI7KGD 发给 PI4DX 的响应。截图右侧窗口比左侧 Band Activity 显示的时间旧，不能从右侧列表推出解码器在 11:08 停止。

## AutoSeq 状态的证据边界

源码 DecodedText::CQersCall()、deCallAndGrid() 和 DisplayText::displayDecodedText() 的一般处理会把非 CQ 标准消息的第二词作为历史目标。若完成消息识别启用，RC2SD PI4DX 73 可以使 PI4DX 的 QsoHistory 状态进入 RFIN；这影响的是 PI4DX 的历史状态，不是 BI7KGD 的响应状态。R+08 不符合该完成状态路径，也不是对 BI7KGD 的 RREPORT。

这条解析结论是代码路径事实；完成标记及 AutoSeq 设置在事件时是否启用无法由当前 JTDX.ini 还原。MainWindow 的回答 CQ 重试计数阈值只有在对应目标状态和计数条件满足时才清理目标。日志观察到一次发射，没记录当时 QsoHistory 内部 count，因此“计数阈值没有达到”是合理解释而不是事件时的已证实内部状态。

P029 基线已经包含 AutoSeq 重试计数与冷却修复。本批没有改候选优先级、解析规则、计数、目标选择、发射阶段、PTT 或恢复策略；源码变化只增加诊断观察和只读快照。

## CAT 故障通知路径

静态源码显示的预期路径如下：

1. HamlibTransceiver::check_poll_read() 记录硬失败决策及 `throws_to_offline`，并调用现有 error_check()。异常沿轮询调用返回到 PollingTransceiver::poll() 的 catch 分支。
2. PollingTransceiver::poll() 把异常转换为现有 offline(message) 调用；TransceiverBase::offline() 在 emit 前记 `stage=entered`，Q_EMIT 返回后才记 `stage=after_emit`。后者证明信号发射语句已返回，不证明接收端 slot 已运行。
3. Configuration::impl 创建专用 transceiver_thread_，Rig 对象由 transceiver_factory_.create(..., transceiver_thread_) 放入该线程。故障连接写成 connect(rig, failure, this, slot)，没有显式 Qt::ConnectionType；跨线程时采用 Qt 默认 AutoConnection 投递规则。
4. Configuration::impl::handle_transceiver_failure() 先执行 close_rig()，随后看配置窗口可见性。可见时显示 Rig failure 对话框，不向 MainWindow 转发；不可见时发出 Configuration::transceiver_failure。
5. MainWindow::handle_transceiver_failure() 收到通知后判断 AutoSeq 是否支持、当前目标和 TX 意图，必要时创建/保留恢复票据；然后沿现有 haltTx() 与 rigFailure()/限次重连路径处理。
6. MainWindow::handle_transceiver_update() 只有在收发机在线且 PTT-off 更新到达时，才按现有逻辑释放 DX 并等待新解码。PTT-on 更新会保留票据和 DX；上下文变化会取消恢复票据。

上述是源码控制流，不证明本次运行经过了这些分支。旧 jtdx_recovery.log 在 EPROTO 后没有 MainWindow 自动恢复、计划重连、重连在线或 PTT-off 释放记录。解码日志继续写入并不能证明 CAT 故障信号已送达 UI 线程。Configuration 可见分支是源码中存在的可能解释，但没有事件日志证明当时配置窗口可见。没有证据证明模态对话框或 Qt 重入导致信号丢失。

## 本批只读诊断日志补强

为下次故障建立同一受限 jtdx_recovery.log 的可检查链，本批只增加状态迁移日志：

- CAT 硬轮询决策是否进入 offline 异常路径；Polling catch 开始 offline 转换；TransceiverBase::offline() 的进入与 Q_EMIT 前后阶段。
- Configuration slot 入口、`close_rig()` 返回后的阶段，以及后续窗口可见值和转发决策。
- MainWindow 是否收到通知；恢复票据是创建、保留、拒绝还是取消及原因；当前目标、主状态、QsoHistory 状态/重试数和 TX 意图。
- 重连在线更新中的 PTT 观测值，以及保留票据、取消上下文或在 PTT-off 后释放 DX 的既有决策。
- 所有现有票据取消入口统一记录静态原因码；不改变原有调用顺序和策略。

QsoHistory::diagnosticSnapshot() 是只读快照，仅暴露指定呼号当前状态和尝试计数；缺失时明确记 history_known=false，不补造值。第三方 73 的来源与目标归属没有低耦合状态迁移接口；本批不改解码解析接口，也不为其增加跨层依赖。

新日志不记录故障文本、CAT 原始命令、串口端口或凭据；继续使用 JtdxLocalLog 的一行 UTF-8 格式、256 KiB 活动文件上限和单个轮转文件。`JtdxLocalLog::append()` 现在在同一保护范围内执行互斥锁获取、路径/行格式化和文件轮转/写入；异常仍由 `QMutexLocker` 栈展开释放锁，并返回 false。锁的粒度、轮转次序和文件上限逻辑没有改变。MainWindow 的新日志使用延迟格式化保护，CAT 轮询和 Configuration/Transceiver 边界也捕获格式化与写入异常；票据取消先执行原 `cancel()`，再尝试落日志。不打开任何解码调试设置，不读取真实 CAT/PTT/TX。

`jtdx_local_log_test` 用不可写入路径验证 `append()` 返回 false 且不抛异常，并验证日志回调写入失败时控制请求仍按原回读完成；`autoseq_cat_diagnostics_contract_test` 检查取消处于诊断异常保护之外、入口/emit 阶段顺序和原信号连接/释放门槛。该证据覆盖本地代码路径，不模拟真实存储故障或硬件。

## 合成完整日志链与缺段解释

以下是字段与阶段的合成示例，不是本次真实 CAT 运行记录；时间、设备状态和票据结果仅演示格式：

```text
2026-09-28T10:00:00.100 [rig-control] ftx1-cat-poll operation=protocol_sync rc=-8 category=EPROTO ... decision=hard-failure throws_to_offline=true
2026-09-28T10:00:00.101 [rig-control] poll_exception offline_transition=begin reason_present=true
2026-09-28T10:00:00.102 [rig-control] transceiver_offline stage=entered failure_signal=not_yet_emitted reason_present=true
2026-09-28T10:00:00.103 [rig-control] transceiver_offline stage=after_emit failure_signal=emitted reason_present=true
2026-09-28T10:00:00.104 [rig-control] configuration_failure stage=entered received=true reason_present=true
2026-09-28T10:00:00.110 [rig-control] configuration_failure stage=after_close_rig received=true reason_present=true
2026-09-28T10:00:00.111 [rig-control] configuration_failure stage=decision received=true configuration_visible=false forward_to_main_window=true reason_present=true
2026-09-28T10:00:00.112 [auto-call] recovery_ticket decision=created reason=active_tx_intent was_pending=false pending=true target=PI4DX ...
2026-09-28T10:00:00.113 [rig-control] failure_received=true reason_present=true online=false ptt=true ...
2026-09-28T10:00:05.000 [auto-call] recovery_online_update online=true ptt_known=true ptt_on=false target=PI4DX ... decision=release_dx_wait_fresh_decode
```

解释缺段时要考虑日志本身也可能写失败：只有 `stage=entered` 而没有 `after_emit`，表示没有观察到 Q_EMIT 之后的阶段；不能仅凭缺段断定 signal 未发射。Configuration 有 `entered` 却没有 `after_close_rig`，可能是 `close_rig()` 尚未返回/异常退出，也可能是该条日志写失败。若 decision 记录 `configuration_visible=true forward_to_main_window=false`，没有 MainWindow 收到记录符合既有分支；若记录 `forward_to_main_window=true`，之后缺少 MainWindow 记录才表明通知尚未被该日志证实收到，仍须考虑异步队列和日志写失败。MainWindow 在线 PTT-off 行只会在恢复票据等待重连的路径出现；缺失并不能单独区分未重连、未收到该状态或日志写失败。

## 本批实施结果

本批仅补充观察日志和只读状态快照：`HamlibTransceiver.cpp`、`PollingTransceiver.cpp`、`TransceiverBase.cpp`、`Configuration.cpp` 记录 CAT 故障链各阶段；`mainwindow.cpp/.h` 记录恢复票据决策、取消原因码和重连 PTT 状态；`qsohistory.cpp/.h` 增加只读诊断快照；`JtdxLocalLog.cpp/.hpp` 保护诊断写入异常。未改候选优先级、消息解析、目标选择、重试计数策略、TX/PTT、故障信号连接或投递方式、重连策略和 DX 清理门槛；没有版本号、配置或打包改动。

### 取消调用行为等价核对

以下各处仍在原条件成立时调用同一个 `AutoSeqRecoveryPolicy::cancel()`。统一 helper 只采集取消前状态；状态读取/日志格式化异常会被捕获；`.cancel()` 无论诊断是否成功都会执行。对有条件调用，helper 内取消前的 pending 检查只控制是否记一条取消日志，不再控制是否真正取消。

| 调用处 | 原取消条件/位置 | 核对 |
|---|---|---|
| AutoSeq 按钮 | 进入 `!checked` 分支时无条件取消 | 分支与取消顺序保持。 |
| Enable Tx 按钮 | `!checked && pending && !internalHalt && !internalUiChange` | 条件保持。 |
| Shift+E、Ctrl+E | 各自按键分支内 `pending` | 条件和两个按键路径保持。 |
| `process_Auto()` | `pending` 且恢复记录的 band/mode 与当前上下文不同 | 条件保持；原因码日志带取消前快照，在策略取消后写入。 |
| TX period 按钮 | `pending` | 条件保持。 |
| `clearDX()`、DX 呼号编辑 | `pending && !internalUiChange` | 两处条件保持。 |
| `switch_mode()` | `pending && recoveryMode != currentMode` | 条件保持。 |
| `band_changed()` | `pending && recoveryBand 非空 && recoveryBand != newBand` | 条件保持。 |
| Stop Tx | `pending && !internalHalt` | 条件保持。 |
| CAT 恢复在线更新 | `online && awaiting_reconnect && changedContext` 分支 | 原分支和取消点保持。 |
| CAT 故障处理 | 不支持恢复且 `pending` 时取消；支持恢复时，仅原 `preserveIntent || pending` 分支调用 `disconnected()` | 两个决策分支保持；`pending()` 是无副作用只读查询，同一 MainWindow 线程内的局部快照替代重复查询。无票据且无发射意图时仍不创建/取消票据。 |

helper 在落取消日志前调用原策略取消，因此文件 I/O 不会延后该状态变更。其它调用点后续原有 UI/TX 语句顺序未移动。

构建目录为 `local-support/build/AutoSeq-CAT-Diagnostics-20260928`，配置为 Release、MinGW Makefiles、`JTDX_BUILD_LOCAL_TESTS=ON`、`WSJT_ENABLE_OMNIRIG=OFF`，使用本机 Qt、Hamlib 和 FFTW。`cmake --build ... --clean-first --parallel 8` 干净全量构建 `jtdx` 及全部测试目标成功，退出码 0；日志为 `local-support/evidence/AutoSeq-CAT-Diagnostics/full-build.log`。复核后的 7 项定向 CTest 为 7/7 通过，日志为 `targeted-ctest-post-review.log`；初次实施时的原始 7/7 日志也保留在 `targeted-ctest-final.log`。

`ctest --test-dir ... --output-on-failure` 全量运行 31 项，30 项通过、1 项失败，最终日志为 `full-ctest-final.log`。唯一失败是未修改的 `jtdx_web_server_test` 断言自动端口搜索要跳过占用端口与数值 UDP 端口；该测试源文件和 Web Server 实现不在本批 diff 中。两项最初因 CTest 环境缺少本地 Hamlib DLL 路径而报 `0xc0000135`；将 `local-support/dependencies/hamlib-4.7.2/bin` 加入 PATH 后单独复验均通过，结果见 `runtime-dependency-ctest.log`。没有为通过全量测试而改动无关端口行为。

配置时曾用 Windows 反斜杠路径导致 RC 编译器路径转义失败；将 CMake 路径改为正斜杠后独立重新配置成功。最终配置日志记录的是成功那次配置。构建目录和验证日志均限于源码树 `local-support`，没有散落到源码文件目录。

本批未启动构建出的 JTDX，也未操作 CAT/PTT/TX、真实电台或配置窗口；没有做 HIL 或生产验证。全量构建与 CTest 只证明本机编译及现有自动化测试结果，不证明真实故障时 Qt 跨线程通知和硬件重连行为。新增 recovery 日志不写 CAT 故障文本、原始命令、串口端口或凭据；代码原有的 `haltTx()` 调试事件仍受用户现有 `write_decoded_debug` 设置控制，本批没有开启该设置或改变其既有输出。

未决问题仍是事件时 Configuration 是否可见、失败是否转发至 MainWindow、MainWindow 当时持有的实际历史计数，以及 CAT 断开后的重连/PTT-off 读取结果。需要下一次自然发生的故障或明确提供新的运行日志后，才能用新增状态链确认。
