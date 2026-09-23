# AutoSeq DX 占用与重试终止修复

## 问题与目标

本次修复针对自动回答 CQ 时 DX 目标长期占用的问题。方向性 CQ 在活动呼号读取路径中曾把历史尝试数改写为 1，因此计数器到达阈值的条件可能永远无法成立。另一处策略在回答 CQ 次数达到上限后，仍可能因优先级较高且关闭严格方向 CQ 而返回“无动作”。自动目标清理后，持续 CQ 的同一电台还可能在下一次解码立刻重新入选。

目标是让当前目标的尝试数持续累计、达到启用的回答 CQ 上限后必定释放 DX，并在有限时间内抑制同一呼号的普通自动 CQ 重选。明确面向本台的定向来呼仍可进入独立选择路径。正常 QSO 的有效回应状态不属于回答 CQ 重试终止条件，不会被此清理打断。

## 处理规则

- 方向匹配只影响候选入选。目标一旦进入 AutoSeq，其重试计数沿用 QsoHistory 中该呼号的累计计数，不因之后收到的 CQ 方向而重置。
- RFIN、RCQ、SCALL，以及跳过 TX1 时的 SREPORT，在回答 CQ 计数达到已启用上限后都生成终止清理动作。已配置的自动特殊目标仍进入待机清理；其他回答 CQ 目标走既有清理路径。
- 自动特殊目标达到上限后调用 QsoHistory `calllist()` 并标记记录来源，在五分钟内抑制同报告或较弱报告的普通 CQ 自动入选。超过 300 秒或收到更强报告即可再次竞争；冷却是进程内短期状态，不写入持久配置。一般失败记录仍由既有筛选路径按原规则使用。
- 冷却触发时仅在启用解码调试日志后写一条记录，包含呼号、优先级、重试次数/上限、报告和解码时间；已有逐批状态日志提供候选状态与计数。
- 冷却只抑制 RCQ/RFIN 普通 CQ 候选。两条同时包含 CQ 候选和定向回应的选择路径在启用 rare-target flags 时继续使用原有 `calllist()` 记录；未启用时只应用标记为自动特殊目标失败的记录，不扩大普通目标的既有行为。第三条仅含 RCQ/RFIN 的路径继续使用原有失败记录。RCALL、RREPORT、RRREPORT、RRR、RRR73 始终旁路失败冷却。黑名单、方向匹配和优先级条件仍按原有筛选执行。
- 定向来呼专用筛选仍可在混合候选路径之前选中明确面向本台的回应；即使该路径未启用，混合候选表里的有效 QSO 回应也会旁路普通 CQ 失败冷却。
- 五分钟按解码时间的日内秒数计算，跨午夜按 24 小时回绕处理；300 秒边界仍在冷却内，超过边界后释放。
- 特殊失败记录和候选报告无法解析时均按最弱报告 `-60` 处理，避免 `QString::toInt()` 失败得到 0 后把无效数据当成异常强报告；冷却仍受 300 秒上界约束。

## CAT 恢复与安全边界

本次不改 Hamlib DLL，也不改现有 CAT 恢复状态机。当前恢复逻辑要求电台在线并确认 PTT 关闭后才释放旧 DX，恢复后等待新解码；只有匹配原呼号的新批次状态才可继续原 QSO，未完成的结束消息还要由新解码确认。修复没有引入恢复时直接发射的路径。

未新增独立的“无进展超时”定时器。现有状态没有统一、可证明非 PTT 的周期计时边界；额外定时器可能打断正常 QSO 或在发射切换时清理状态。已实现的回答 CQ 最大次数终止和 CAT 安全恢复释放覆盖本次确认的卡死路径，独立无进展看门狗留待具备可靠状态边界时再评估。

## 验证范围

策略测试覆盖活动呼号计数保持和阈值终止；`autoseq_cooldown_test` 直接调用真实 `QsoHistory::autoseq()`，覆盖三条候选路径、rare flags 开关、特殊/普通 `calllist()` 来源、RCQ/RFIN、真实 RCALL 回应、负/空/非法报告、报告改善、300 秒边界及午夜回绕。定向呼叫与 CAT 恢复既有策略测试也应保持通过。`RRR73` 是状态枚举名称；FT8 解码文本中的完成消息为 RRR 或 RR73。

构建目录：`C:\JTDX64\build-webui-dev-msys2`（Release，MinGW64/MSYS2）。重新配置后执行全量构建 `C:\msys64\mingw64\bin\cmake.exe --build C:\JTDX64\build-webui-dev-msys2 -j 6`，`jtdx` 与全部测试目标构建通过；编译输出只有既有未使用变量、弃用 API 和 `strncpy` 警告。针对性 CTest（`autoseq_cooldown_test`、`autocall_policy_test`、`directed_call_policy_test`、`autoseq_recovery_policy_test`）4/4 通过。完整 CTest 需将 `C:\msys64\mingw64\bin`、`C:\msys64\usr\bin`、`C:\JTDX64\JTDX-2.2.159.2.10-local-05ee60d-P25\bin`、`C:\JTDX64\build-webui-dev-msys2` 按此顺序加入当前进程 `PATH`（P25 `bin` 提供 Hamlib `libhamlib-4.dll`），设置 `QT_QPA_PLATFORM=offscreen` 和 `QT_QPA_PLATFORM_PLUGIN_PATH=C:\msys64\mingw64\share\qt5\plugins\platforms`，再执行 `C:\msys64\mingw64\bin\ctest.exe --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure`，结果 27/27 通过。首次因 `PATH` 缺少 Hamlib DLL 有两项测试以 `0xc0000135` 启动失败；补足构建运行依赖后两项单测及完整套件均通过。本次不操作真实 CAT、PTT、TX 或电台。Hamlib DLL未修改；真实电台联调和长时间运行 HIL 未验证。
