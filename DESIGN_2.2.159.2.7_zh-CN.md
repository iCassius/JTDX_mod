# JTDX 2.2.159.2.7-test 设计记录

## 边界违规纠正（2.2.159.2.7-test 前置约束）

本节先于实现固化本任务的工程边界（需求引用：`codex_delegation`，源会话 `01a03476-cff9-7fd1-86ed-4787d12a1dba`）：

- FTX-1 的电台控制后端始终是 Hamlib model `1051`。不得安装、注册、运行或配置 OmniRig；不得把 OmniRig 作为 FTX-1 修复或新的运行时依赖。不得为了通过构建删除或永久关闭普通 JTDX 的 OmniRig 功能，也不得改变默认 Windows 构建语义。
- 必须区分既有源码中的通用 OmniRig 兼容层、Windows CMake 历史上无条件执行的 COM/`dumpcpp` 依赖门槛，以及本次 FTX-1 Hamlib-only 运行时方案。构建依赖门槛不是 FTX-1 功能方案，不能把缺少 OmniRig 的配置失败误报为 Hamlib 修复完成。
- Windows 构建选项必须明确表达 OmniRig 支持，默认开启；默认 ON 时保留既有源码、COM 检查、注册项和普通 JTDX 行为。仅本次明确的 Hamlib-only 测试包允许显式 OFF；OFF 不得静默自动降级，且配置/编译不得包含 `OmniRigTransceiver` 或 ActiveQt wrapper，不得注册 OmniRig Rig 1/2。Hamlib 及其他正常后端必须继续存在。
- 不新增第三方组件、下载器、安装器、后端、运行时依赖、静默回退或伪成功；任何新增依赖、后端或默认行为必须另行批准。测试包若使用 OFF，必须在 README 和发布说明显著标注其只用于 Hamlib 后端验证，普通 OmniRig 用户应使用默认 ON 的完整源码构建。

## 现状证据

- `PollingTransceiver::handle_timeout()` 在现有 500 ms 事件循环中调用具体实现的 `do_poll()`；异常会直接进入 `offline()`，随后由 `MainWindow::handle_transceiver_failure()` 停止发射并安排重连。
- `HamlibTransceiver::do_poll()` 对 FTX-1 先读取 PTT，再按安全状态决定后续查询；非 FTX-1 仍保持 VFO、Split、频率、模式、表计后读取 PTT 的历史顺序。频率/VFO/PTT 的若干读取使用 `error_check()`，因此一次 Hamlib 负返回值即可中断连接；模式和表计的部分错误则仅保留旧的“忽略并写零/不更新”行为。
- 写路径 `do_frequency()`、`do_tx_frequency()`、`do_mode()`、`do_ptt()` 仍由 `error_check()` 硬失败处理；本版本不改变这些路径，也不改变 Fake It 的切换/恢复语义。
- JTDX 的 CAT 线程与 Qt 轮询定时器已经串行运行。`ptt_on_` 表示当前 PTT 写入意图，`state().ptt()` 是最近一次已确认的实际状态；两者不一致或尚未成功读取 PTT 时，状态按安全敏感处理。
- 现有 `jtdx_recovery.log` 已有约 256 KiB 后轮转 `.1` 的机制；本版本在 Hamlib 层复用同一文件和上限，不承诺原始串口帧。
- 现有硬重连路径会清掉 Enable Tx/当前发射，但 AutoSeq 解码处理仍可能在重连期间被旧周期的解码触发。因此恢复必须单独设置“等待连接恢复后的新周期候选”门槛。

## 最小方案

1. 增加 Qt 无关的 FTX-1 CAT 轮询策略：仅当型号为 Hamlib model `1051`、操作是只读轮询、错误属于可判定瞬时类别（EPROTO/ETIMEOUT/EIO/总线错误），且最近 PTT 已确认关闭、没有 PTT 写请求未确认时，前两次同一操作失败软忽略并保留最后有效值；第 3 次升级到现有异常/重连流程。成功读取清零该操作的连续失败计数。非 FTX-1、写操作和安全敏感阶段保持硬失败。
2. 在 FTX-1 的 500 ms 轮询中先读取 PTT；发射意图/实际 PTT 任一不满足安全待机、PTT 状态未知、两者不一致，或 PTT 开关后的两个轮询窗口内，继续暂停 VFO、Split、频率、模式和其他非必要查询。仅当实际 PTT-on 已确认、请求已确认且两个过渡保持窗口结束后，放行 power/SWR 表计读取；该轮保持 meter-only，不放开其他查询。PTT 读取和所有写命令仍执行原有确认/异常路径，不创建线程。
3. CAT 诊断记录操作名、rc、错误类别、连续次数、FTX-1 标记、PTT 意图/实际/已知状态、安全判定和 soft-ignore/escalate 结果；文件仍以 256 KiB 轮转。
4. 增加 AutoSeq 恢复策略：硬故障前若 AutoSeq/Enable Tx 表示用户仍在自动值机，则记录恢复意图；连接重新 online 后，只有解码开始时间晚于恢复时间且已选出新的候选/定向消息时，才允许重新 arm Enable Tx。旧周期解码只更新显示/历史，不触发发射。

## 验收边界

纯策略测试覆盖单次软忽略、阈值升级、成功清零、非 FTX-1、写失败、PTT 敏感阶段、PTT on/off/过渡期间的 meter-only 边界、可选表计错误、Enable Tx 但实际待机和 AutoSeq 旧解码门槛；完整 CTest、隔离 Windows 构建/打包、PE 依赖闭包和 Hamlib DLL SHA256 另行记录。全程不启动 JTDX、不连接 CAT/PTT/电台、不进行真实发射；真实 FTX-1 的功率/SWR 回读与 UI 刷新仍留给用户 HIL 确认。

## 中期安全自审记录

- `get_ptt`、`get_vfo` 和 FTX-1 其他关键只读操作一旦软失败立即结束当前 `do_poll()`；不在同一轮继续读取可能不一致的 VFO、频率或模式。只有 PTT-first 已确认实际 TX 且两个保持窗口结束时，才进入现有 power/SWR meter-only 路径；表计错误仍复用原有可选错误、计数和完成语义。
- 失败计数按操作保留诊断，同时以每个失败轮次维护总体连续异常计数；交替操作的第三个失败轮次同样升级。只有完整无失败轮次才清零总体计数。
- `do_ptt()` 成功写入后设置两个 500 ms PTT 过渡保持轮次；在首次新 PTT 读确认前，任何读失败都不能软忽略。PTT 请求未确认、实际开启或状态未知也均不满足安全待机；实际 TX 仅可放行 power/SWR 表计，不放行 VFO、Split、频率或模式。
- AutoSeq 恢复门只拦截恢复时间之前的旧解码；恢复时间之后的候选先按现有普通状态机选择。该门本身不加入普通候选的 Enable Tx 分支，只在新候选到达后释放陈旧解码保护。
- `git diff --check`、行尾和变更规模在提交前复核；构建使用独立目录，旧包目录和 Hamlib 源码/构建均不写入。
