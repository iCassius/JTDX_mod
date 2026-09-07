# 石家庄业余无线俱乐部版开发状态

## 内置 Web UI 设计与阶段入口

Web UI 工作从阶段 `P0 文档与事实基线` 开始，当前只完成需求/边界/设计草案/验收矩阵/恢复日志整理，未实现代码，未启动 JTDX，未连接 CAT/PTT/真实电台，未做浏览器、HIL 或部署验证。后续请从 [`WEB_UI_START_HERE_zh-CN.md`](WEB_UI_START_HERE_zh-CN.md) 恢复，并按 `docs/web-ui/PROGRESS_zh-CN.md` 逐阶段推进。Web 必须使用独立 TCP 端口、零新增 Web 进程和常驻线程，不改变既有 UDP 配置；当前程序已有 `jtdxjt9` 解码子进程，该既有边界不属于 Web 新增进程。

## 当前基线

- 产品显示版本：JTDX `2.2.159.2.9`。
- 标题：`JTDX v2.2.159.2.9 自动起呼版 By BI7KGD`。
- Windows 四段版本资源保持可解析的 `2.2.159.2`；`.9` 是本次产品显示后缀，不改变 PE 资源字段布局。
- 源码目录：`C:\JTDX64\jtdx_sourcecode`。
- Hamlib 运行时：`4.7.2`，FTX-1 backend `20251224.0`。
- `159` 是当前使用目录；本次 `159.2.9-cat-recovery-test` 只提供可覆盖 `159` 的 `bin/`、`plugins/`、`share/` 三个运行时目录。用户应先自行备份并关闭相关程序，再自行覆盖现有 `159`；本任务不替用户覆盖或删除既有目录。

## JTDX-CAT-RECOVERY-20260906

本次恢复修复以 Git 基线 `b8d94539fea8a163b137dbd0fe30681e2dca2252` 为依据，结果提交由本地交付记录补入。FTX-1 Hamlib 4.7.2 的 `RIG_DEBUG_ERR`、PTT-first 轮询和错误恢复日志只增加诊断，不放宽 PTT/CAT 安全门，也不替换现有运行时 DLL。

- 硬故障后只有在 CAT 重新在线且实际 PTT 已确认关闭时才释放旧 DX；恢复票据等待新的完整解码批次。
- 新批次优先接受明确发给本台的标准报文，并按 `QsoHistory` 已有 RCALL、RREPORT、RRREPORT、RRR、RRR73、R73 状态恢复 Tx2/Tx3/Tx4/Tx5；完整中断呼号保留在 DX 字段，匹配键单独使用基本呼号。
- 首个 73 的历史记录可能在 TX 开始时已经写成 `FIN`。只有本批实际收到原台的 `RRR`、`RR73` 或 `73` 才允许判断结束；若故障时 Tx5 在途则重发 Tx5，否则不凭旧 `FIN` 再发射。
- 没有原台续联或新候选时 DX 保持为空、票据继续等待；实际新候选才消费恢复票据。用户停发、Escape、UDP HaltTx、禁用、清除、目标/模式/波段/Tx 周期变化均取消票据；内部故障 halt 保留票据。
- `jtdx_recovery.log` 的 Hamlib 错误记录与 FTX-1 聚合失败计数使用同一受限日志轮换；日志互斥只覆盖 Hamlib 写入者，不宣称覆盖 MainWindow 的全局线程安全。

本 `.9` 构建明确使用 `WSJT_ENABLE_OMNIRIG=OFF`，沿用 Hamlib-only 交付边界；默认选项仍保持 `ON`。以实现提交 `e5a4381e6ac496c7d1a0088e39c1e3a08d94ad2a`、文档结果提交/构建 artifact code HEAD `651bf9f674d0e028ed0a6e03e5e7cd86c72701bc` 为依据，权威 `build-159.2.9` MinGW64 Release 构建完成，最终 CTest `13/13` 通过（总耗时 22.00 秒，含 `autoseq_recovery_policy_test` 与 `omnirig_build_option_test`）。

最终交付 ZIP 为 `C:\JTDX64\159.2.9-cat-recovery-test.zip`，大小 56,071,897 字节，压缩包共 75 条记录，顶层目录严格为 `bin`、`plugins`、`share`；同级 SHA256 sidecar 为 `2b7f75bf3987490583b06eb1fce7ef92608dfc794046e0c071d4547affc005ab`。包内 `jtdx.exe` 与权威构建产物一致，Hamlib 运行时 DLL 与既有 4.7.2 运行时一致。未启动 JTDX，未连接 CAT/PTT，未操作真实电台，也未覆盖 `C:\JTDX64\159`。

本任务的 Hamlib 4.7.2 源码分析锚点：

- [ftx1_tx.c](https://github.com/Hamlib/Hamlib/blob/4.7.2/rigs/yaesu/ftx1/ftx1_tx.c)
- [ftx1_vfo.c](https://github.com/Hamlib/Hamlib/blob/4.7.2/rigs/yaesu/ftx1/ftx1_vfo.c)
- [newcat.c](https://github.com/Hamlib/Hamlib/blob/4.7.2/rigs/yaesu/newcat.c)

## 2.2.159.2.8 历史验证记录

以下内容记录前一版 `.8` 自动起呼清理包的构建、验证和 HIL 边界，不作为本次 `.9` CAT 恢复包的结果声明。

## FTX-1 与 OmniRig 构建边界

- FTX-1 始终使用 Hamlib model `1051`；本任务不得安装、注册、运行或配置 OmniRig，也不得把 OmniRig 作为 FTX-1 修复或新的运行时依赖。
- `WSJT_ENABLE_OMNIRIG` 是明确的 Windows 构建选项，默认 `ON`。默认 ON 保留既有 OmniRig 源码、ActiveQt/COM 检查、Rig 1/2 注册和普通 JTDX 行为；Windows CMake 的依赖门槛不等于 FTX-1 功能方案。
- 本 `2.2.159.2.8` 包是显式 `WSJT_ENABLE_OMNIRIG=OFF` 的 Hamlib-only 测试包：配置/编译不包含 `OmniRigTransceiver`、`dumpcpp` 或 ActiveQt wrapper，不注册 OmniRig Rig 1/2；Hamlib、TCI、HRD、DXLab 等正常后端仍保留。普通 OmniRig 用户应使用默认 ON 的完整源码构建。
- 不新增第三方组件、下载器、安装器、后端、运行时依赖、静默回退或伪成功；新增依赖、后端或默认行为必须另行批准。

## 自动起呼

自动起呼沿用 JTDX 的解析和 AutoSeq 状态机：

`DisplayText::displayDecodedText` → `QsoHistory::message` → `MainWindow::process_Auto`

设置页和主界面“自动程序”菜单提供六个独立选项，默认关闭：

1. 自动起呼新 DXCC；
2. 自动起呼 DXCC 新波段/模式；
3. 自动起呼新网格（priority 15/16，日志中从未通联过的四位网格）；
4. 自动起呼新网格波段/模式（priority 13/14，当前波段或模式尚未通联）；
5. 自动起呼新呼号；
6. 自动起呼新呼号波段。

另有独立的“待机时自动应答呼叫本台”选项，默认关闭。AutoSeq 开启且无进行中 QSO 时，它只接受严格解析出的 RCALL/RREPORT，选择单个来呼、按 `b_time` 同步互补周期、生成 Tx2/Tx3 后开启 Enable Tx；CallNone 仅放宽入站选择，不开启任何出站 CQ 搜索。

新网格与新网格波段/模式两项可以独立或同时启用，同时启用时按两组 priority 并集筛选；旧 `SeqAutoCallNewGrid` 的语义已收窄为 15/16，新 `SeqAutoCallNewGridBandMode` 缺键或升级时默认关闭，不从旧网格选项迁移为开启。两项都继续服从现有触发消息、AutoSeq、Enable Tx、次数限制、清除 DX 和周期同步逻辑。

候选必须是已解析的 `CQ`、`RRR`、`RR73` 或独立 `73`。独立字段判断不会把呼号或普通文本中包含的 `73` 当成结束消息。命中所选条件时，程序自动选择呼号、开启 Enable Tx，并在下一次允许的发射周期起呼；仍需用户先启用主界面的“自动程序/AutoSeq”。

自动起呼周期缺陷的根因已经确定：候选选择返回的 `QsoHistory::b_time` 没有在打开 Enable Tx 前同步到 `m_txFirst`，导致新目标继承旧目标周期。本版本只对自动程序新选出的 `RCQ/RFIN` 候选（包括强制新网格候选）按目标接收时段计算相反 TX 周期；进行中的 QSO、对方直接呼叫、手工双击、Hound/WSPR 不受影响。周期同步使用无副作用的 `setChecked()`，随后重建标准消息，不会误触发手工按钮的清除 DX 逻辑。FT8 的 `00/30 → 15/45`、`15/45 → 00/30`、跨午夜和 FT4 7.5 秒策略均有纯单元测试。

新网格候选在 DX 输入框仍保留旧台信息时也会重新评估，不再要求用户先点击“清除 DX”：仅新网格重评估 15/16，仅新网格波段/模式重评估 13/14，两项同时启用时覆盖 13–16；没有网格、没有保留 DX 或 priority 不属于已启用组时不重触发。DXCC 新波段/模式、新网格波段/模式和新呼号波段可以单独启用，不依赖“显示通知”页相应的显示选项；自动选项仅为候选计算精确启用所需 matcher。

本次 2.2.159.2.5 修复新网格强制重评估的竞态边界：同一批解码中，其他台的 RRR/RR73/73 完成消息不会覆盖仍在进行的 DX QSO。保留 DX 只有在该 QSO 已实际发送 Tx5，且当前周期没有等待对方首个 73、没有正在发射、普通 AutoSeq 尚未处理时，才允许进入新的候选选择；这些条件由 `AutoCallPolicy::canForceCandidate` 纯策略测试覆盖。

本次回归修复补齐自动新目标 `RFIN` 状态的回答 CQ 次数上限：它对应首次 TX1，达到上限后与 `RCQ/SCALL` 一样清理 DX 呼号和网格，使后续新网格或新呼号候选重新进入 AutoSeq 选择。该边界由 `AutoCallPolicy::answerCQRetryLimitReached` 测试；Hound、活动发射、pending first 73、已处理周期和 Tx5 后候选抢占保护保持原有约束。

本次 `JTDX-AUTOSEQ-CLEANUP-20260830` 将回答 CQ 终止判定集中到 `AutoCallPolicy::answerCQRetryAction`：它先统一处理 `RFIN`、`RCQ`、`SCALL` 和跳过 TX1 时的 `SREPORT`，再按 Hound、阈值和“转呼他台”判定 `none`、普通收尾或自动目标待机收尾。阈值只受回答 CQ 次数开关控制；转呼终止是独立 OR 条件，但仍须满足既有 `HaltTxReplyOther` 或频谱与当前 TX 保护范围重叠语义。自动目标优先级统一由 `isConfiguredAutomaticTarget` 覆盖 22/23、20/21、15/16、13/14、7/8、5/6，不再由 `process_Auto` 的互斥优先级门漏掉高优先级目标。

无回应次数继续使用现有的呼入应答/发送报告次数设置。自动目标达到阈值或收到符合既有停发条件的转呼信号后，只调用一次 `haltTx`（若 `readFromStdout` 已提交停发则只清 DX/重置），再调用一次 `clearDX` 和对应 `reset_count`，回到 `CALLING` 待机且本轮不重新选择候选；自动目标不写 `calllist`，避免旧解码立即重入，也不会永久屏蔽后续新的有效解码。普通非自动目标继续使用原有 `calllist`、`m_counter`、转呼他台和 single-shot 语义。Hound/WSPR、定向呼叫、手工双击、活动发射、pending first 73、已处理周期和 Tx5 后候选抢占保护不改变。

该组合边界由 `autocall_policy_test` 覆盖：六组自动优先级的阈值待机动作、`RFIN/SCALL/SREPORT`、阈值内、计数关闭、独立转呼终止、`HaltTxReplyOther`/频谱重叠开关、Hound、普通路径和无终止动作；既有 AutoSeq 抢占保护测试继续通过。仅完成离线自动测试和 clean Release 构建，未启动 JTDX、未连接 CAT/PTT、未操作真实电台或发射，真实自动起呼次数和 FTX-1 HIL 仍需人工确认。

## Fake It 和拨盘频率

Fake It 的临时发射拨盘频率与接收拨盘频率由 `EmulateSplitTransceiver` 分离管理：

- 发射时底层电台可以临时切到 TX 拨盘频率，但上层和主界面保持显示原 RX 拨盘频率；
- TX→RX 时忽略迟到的 TX 频率回报，直到收到 RX 频率恢复确认；
- 连续收到多个 RX 请求不会提前结束恢复状态；
- 稳定进入 RX 后重新跟随真实 VFO，允许用户手动旋转电台频率；
- 对无法可靠回读 PTT 的后端，Fake It 使用程序请求的 PTT 状态；
- 错误中止会先向上层报告安全的 RX/非 PTT 状态，再进入 CAT 错误恢复。

该修复避免把 21.075 MHz 的临时 TX 拨盘值吸收成新的 21.074 MHz 接收基准。纯状态机测试已经覆盖正常 TX、重复 RX 请求、迟到 TX 更新、RX 确认、手动 VFO 和错误恢复；真实 FTX-1 时序仍需 HIL 验证。

## 长时间挂机恢复

### 音频输出

问题二目前没有足够证据证明真实根因是声卡或 Hamlib；本版本做的是安全硬化和诊断。Windows 下每次开始发射前仍重新枚举并重建 `QAudioOutput`，设备选择先匹配精确设备对象，只有对象失效且名称唯一时才回退；同名端点超过一个时拒绝猜测。启动后必须收到 `QAudioOutput::ActiveState` 才发出 ready，并设置 500 ms 有界确认超时，同时记录 `processedUSecs`/实际进度（不把正常起始静音直接判为失败）。

启动确认失败时立即停止 Modulator、标记下次必须重建、沿现有 haltTx 路径降低 PTT 并关闭 Enable Tx；本版本不在同一时隙重试，避免无界或静默发射。`jtdx_recovery.log` 以单次发射少量记录关联自动目标/计划周期、PTT 请求与确认延迟、设备名、QAudio 状态、启动确认/进度或失败原因。

### CAT

CAT 打开失败或运行中断后按 `2 秒 → 5 秒 → 15 秒` 有限重连，只有收到真实在线状态才重置恢复计数。启动时的首次 CAT 打开被延迟到主窗口进入事件循环后执行，因此串口占用或电台不可用时主窗口仍可显示，用户可以进入设置修改端口。

恢复事件写入数据目录的 `jtdx_recovery.log`，包含错误原因、RX/TX 频率、PTT、Split、发射和 Enable Tx 状态；日志约 256 KiB 后轮换。

FTX-1（Hamlib model `1051`）的 `do_poll()` 保持 PTT-first：PTT 未知、请求与实际不一致、PTT 切换后的两个保持轮次内，继续暂停 VFO、Split、频率、模式和其他非必要查询。实际 PTT-on 已确认、请求已确认且保持结束后，只进入现有 power/SWR meter-only 读取路径，恢复发射期间表计刷新；不改变其他型号，也不改变 Hamlib 源码或 DLL。纯策略测试覆盖 PTT on/off、过渡保持、meter-only 放行、可选错误和连续瞬态错误边界；真实电台读数和界面刷新仍需 HIL。

## 其他界面功能

- 中国普通呼号继续显示省级归属地，例如 `中国 河北`；归属地只用于显示，不改变 DXCC、日志 B4 或自动起呼资格。
- 左右解码窗口可在呼号上右键选择“在 QRZ.com 查询”，通过安全的 `https://www.qrz.com/db/<CALL>` 地址打开浏览器。
- 主窗口和主要控制按钮增加最小尺寸，修复小窗口下按钮被压缩到文字遮挡的问题。
- 关于窗口和标题栏显示 `2.2.159.2.8 自动起呼版 By BI7KGD`。

## UDP 遥测镜像

- 主 UDP 继续用于桥接/WebUI/MCP，并保留现有反向控制、PSK Reporter 和输入控制连接。
- 辅助 UDP 使用独立的 `MessageClient`、socket、schema 状态和 heartbeat，只读镜像主通道的 Heartbeat、Status、Decode、WSPRDecode、Clear、QSOLogged、LoggedADIF 和 Close。
- `EnableUDP2adifBroadcast`、`UDP2Server`、`UDP2ServerPort` 键名保持兼容；旧的辅助 UDP 裸 ADIF（默认 2333 端口）语义已改为正式 JTDX/WSJT-X 协议镜像，LoggedADIF 是否发送仍跟随主 UDP 的 `EnableUDP1adifSending`。
- 辅助客户端不连接任何 Reply、Replay、HaltTx、FreeText、HighlightCallsign、SetTxDeltaFreq 或 TriggerCQ 控制信号；地址解析失败、主副目标相同和辅助通道故障均只记录非阻塞诊断，不影响主 UDP、界面、自动起呼、音频或 CAT。

## 自动验证与边界

本次 2.2.159.2.8 的本地自动验证包括：

- 使用 `C:\msys64\mingw64\bin` 的 MinGW64 工具链，在独立 clean Release 目录以 `WSJT_ENABLE_OMNIRIG=OFF` 完成配置和完整构建；
- 完整 CTest 共 `12/12` 通过，含 `omnirig_build_option_test`：OFF 不缓存 `dumpcpp`，目标元数据不含 OmniRig/ActiveQt wrapper，Hamlib 及其他正常后端仍在；
- 默认 ON 的独立配置探针保留 `dumpcpp -getfile` 与 Qt5 ActiveQt/COM 路径；未进行 ON 编译、未启动 JTDX，普通 Windows 构建语义仍由默认 ON 保持；
- `callsignlocation_test`；
- `autocall_policy_test`；
- `auto_tx_period_policy_test`；
- `directed_call_policy_test`，覆盖 CallNone 入站选择、RCALL/RREPORT、确定性同周期选择、孤立完成消息、旧来呼不排队和 Hound/WSPR/TX 守卫；
- `audio_policy_test`；
- `recovery_policy_test`；
- `fake_it_state_test`；
- `qrz_lookup_test`；
- `ui_contract_test`；
- `udp_mirror_test` 使用本机离线 UDP 捕获验证双通道 Heartbeat、Status、Decode、WSPRDecode、Clear、QSOLogged、LoggedADIF、Close、动态切换、禁用后的 heartbeat 静默、双向无效地址隔离和同目标实际 datagram 去重；`ui_contract_test` 验证辅助通道只读连接结构。
- Windows PE 四段版本资源证据（`FileVersion=2.2.159.2`、显示版本 `2.2.159.2.8`）；交付目录需从最终提交完成依赖闭包、SHA256 和 ZIP 根目录检查。

最终交付为 `C:\JTDX64\159.2.8-autoseq-cleanup-test` 及同级 ZIP；包内仅含可直接覆盖现有 `C:\JTDX64\159` 的 `bin/`、`plugins/`、`share/`，不包含启动/验收脚本、README、发布说明或构建产物。用户应自行完成备份、停用相关程序和覆盖操作。

`AUTO_VERIFIED` 不等于真机验证。以下仍需用户 HIL：真实 FTX-1 的 CAT/PTT、power/SWR 回读和 UI 刷新、meter-only 与 PTT 过渡时序、Fake It 时序、串口被其他程序占用时的启动交互、长时间挂机声卡恢复、实际自动起呼发射次数（包括两组网格 priority 的真实日志命中）、PTT/音频时序，以及不同 DPI/窗口尺寸下的界面观感。本版本不启动 JTDX、不操作 CAT/PTT/电台、不发射；`audio_policy_test` 只验证纯策略；Qt 真实音频端点和 processed 音频帧没有被伪造为自动 HIL。
