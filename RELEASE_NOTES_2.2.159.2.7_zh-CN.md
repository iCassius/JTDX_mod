# JTDX 2.2.159.2.7-test 发布与测试说明

显示标题为：`JTDX v2.2.159.2.7 自动起呼版 By BI7KGD`。Windows PE 四段版本仍为 `2.2.159.2`，`.7-test` 为显示和交付标识。

## 重要构建边界

本包使用明确的 `WSJT_ENABLE_OMNIRIG=OFF`，是只用于 Hamlib 后端验证的 Hamlib-only 测试包，不面向普通 OmniRig 用户：

- FTX-1 的后端始终是 Hamlib model `1051`。不得安装、注册、运行或配置 OmniRig，不得把 OmniRig 作为 FTX-1 修复或新的运行时依赖。
- OFF 配置不查找或执行 `dumpcpp`，不编译 `OmniRigTransceiver`，不查找、链接或生成 ActiveQt wrapper，不注册 OmniRig Rig 1/2；Hamlib、TCI、HRD、DXLab 等正常后端仍保留。
- 源码的 `WSJT_ENABLE_OMNIRIG` 默认值为 `ON`。默认 ON 保留既有普通 JTDX OmniRig 源码、COM 检查、注册项和行为；构建依赖门槛不等于 FTX-1 功能方案。
- 不新增第三方组件、下载器、安装器、后端、运行时依赖、静默回退或伪成功；新增依赖、后端或默认行为必须另行批准。

## FTX-1 CAT 变更

- 只对 Hamlib model `1051`（Yaesu FTX-1）生效；其他电台保持原行为。
- 待机且已确认 PTT 关闭时，单次 `EPROTO`、`ETIMEOUT`、`EIO` 或总线瞬时错误仅对只读轮询软忽略，保留最后有效值并在下一轮重试。
- 同一操作连续 3 次，或交替只读操作形成连续 3 个失败轮次，会升级到既有错误/重连流程；成功轮询清零相应计数，完整成功轮次清零总体计数。
- `get_ptt` 失败、`get_vfo` 失败以及其他关键只读操作软失败都会立即结束当前轮询，避免用不一致上下文覆盖缓存值。
- PTT 请求、实际发射、PTT 未知和 PTT 切换后的确认窗口均不软忽略；`set_ptt`、频率、VFO、模式及 Fake It 切换/恢复等写失败仍立即硬失败。
- FTX-1 轮询先读 PTT；PTT 开启期间以及 PTT 切换后的两个轮询周期继续暂停非必要 VFO、Split、频率、模式等查询。仅当实际 PTT-on 已确认、请求已确认且过渡保持结束后，放行现有 power/SWR meter-only 路径，恢复发射期间功率/SWR 刷新。

Hamlib 源码和 DLL 均未修改；日志记录 JTDX 层操作名、rc/类别、连续次数、FTX-1 标记和 PTT 安全字段，并复用 `jtdx_recovery.log` 的约 256 KiB 轮转。日志不包含 Hamlib 原始串口帧。

## 自动验证

在不启动 JTDX、不连接 CAT/PTT/电台、不进行真实发射的条件下，使用 `C:\msys64\mingw64\bin` 的 MinGW64 工具链完成独立 clean Release 构建和完整 CTest：`12/12` 通过。新增 `omnirig_build_option_test` 验证 OFF 不缓存 `dumpcpp`、目标元数据不含 OmniRig/ActiveQt wrapper，Hamlib 及其他正常后端仍在；独立 ON 配置探针验证默认 ON 仍保留原 `dumpcpp -getfile` 与 ActiveQt/COM 路径。

真实 FTX-1 CAT/PTT、功率/SWR 表计回读和 UI 刷新、meter-only 与 PTT 过渡时序、Fake It、音频时序、长时间挂机和自动起呼发射仍需用户进行真机 HIL；自动测试不等同于真机验证。

## 启动与验证

将包解压到独立目录后运行包内 `JTDX-159.2.7.cmd`。脚本以自身目录为根，不写死用户目录。`verify-159.2.7.cmd` 可离线检查依赖闭包、PE 四段版本、相对路径启动和 Hamlib DLL SHA256。
