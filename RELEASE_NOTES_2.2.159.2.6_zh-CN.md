# JTDX 2.2.159.2.6-test 发布与测试说明

显示标题保持：`JTDX v2.2.159.2.6 自动起呼版 By BI7KGD`。Windows PE 四段版本仍为 `2.2.159.2`，`.6` 仅是显示和交付标识。

## FTX-1 CAT 变更

- 只对 Hamlib model `1051`（Yaesu FTX-1）生效；其他电台保持原行为。
- 待机且已确认 PTT 关闭时，单次 `EPROTO`、`ETIMEOUT`、`EIO` 或总线瞬时错误仅对只读轮询软忽略，保留最后有效值并在下一轮重试。
- 同一操作连续 3 次，或交替只读操作形成连续 3 个失败轮次，会升级到既有错误/重连流程；成功轮询清零相应计数，完整成功轮次清零总体计数。
- `get_ptt` 失败、`get_vfo` 失败以及其他关键只读操作软失败都会立即结束当前轮询，避免用不一致上下文覆盖缓存值。
- PTT 请求、实际发射、PTT 未知和 PTT 切换后的确认窗口均不软忽略；`set_ptt`、频率、VFO、模式及 Fake It 切换/恢复等写失败仍立即硬失败。Enable Tx 单独打开但实际处于已确认待机不构成强制中断理由。
- FTX-1 轮询先读 PTT；PTT 开启期间以及 PTT 切换后的两个轮询周期继续暂停非必要 VFO、Split、频率、模式等查询，复用现有 Qt 事件循环，不创建新线程。仅当实际 PTT-on 已确认、请求已确认且过渡保持结束后，放行现有 power/SWR meter-only 路径，恢复发射期间功率/SWR 刷新；不放开其他非必要查询。

Hamlib 源码和 DLL 均未修改；日志记录 JTDX 层操作名、rc/类别、连续次数、FTX-1 标记和 PTT 安全字段，并复用 `jtdx_recovery.log` 的约 256 KiB 轮转。日志不包含 Hamlib 原始串口帧。

## AutoSeq 恢复边界

硬重连会保留用户仍在自动值机的意图，但旧周期解码不会直接触发发射。连接恢复后必须先收到恢复时间之后的新候选/定向消息；恢复门只释放陈旧解码保护，不新增普通候选的 Enable Tx 路径，随后仍完全服从原有 AutoSeq、稀有目标、定向应答、Tx5/Tx1 和 PTT 安全逻辑。

## 自动验证

本次回归补丁在不启动 JTDX、不连接 CAT/PTT/电台、不进行真实发射的条件下，用 `C:\msys64\mingw64\bin` 的 MinGW64 工具链独立编译并通过 `ftx1_cat_policy_test`（含 `-Wall -Wextra -Werror`）。测试覆盖 FTX-1 待机瞬时失败、连续阈值、成功清零、非 FTX-1、写失败、PTT 敏感阶段、PTT on/off/过渡期间的 meter-only 边界、可选表计错误、Enable Tx 待机、交替失败总体阈值以及 AutoSeq 旧解码门控。隔离 Windows CMake 配置在本机缺少 OmniRig、`dumpcpp` 未返回 AXSERVER 的门槛停止；完整 CTest/Release 构建待后续具备相关依赖后执行。

真实 FTX-1 CAT/PTT、power/SWR 表计回读及 UI 刷新、meter-only 与 PTT 过渡时序、Fake It、音频时序、长时间挂机和自动起呼发射仍需用户进行真机 HIL；自动测试不等同于真机验证。

## 启动

将包解压到独立目录后运行包内 `JTDX-159.2.6.cmd`。脚本以自身目录为根，不写死用户目录。`verify-159.2.6.cmd` 可离线检查运行时文件、版本文本和 Hamlib DLL SHA256。
