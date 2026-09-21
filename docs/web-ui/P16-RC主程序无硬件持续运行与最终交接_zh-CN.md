# P16 RC 主程序无硬件持续运行与最终交接（2026-09-21）

## 结论

已使用 P14 解压 RC 中的真实 `bin\jtdx.exe` 完成无硬件主程序持续运行：实际 `1814.2` 秒、30 次采样，进程保持存活，无网络连接、无崩溃或提前退出。结束时因当前工具不提供受支持的原生 MainWindow 关闭操作，仅对本批确认路径和 PID 的自有进程做了受控终止；因此“正常 MainWindow 关闭、退出资源释放、设置保存重载和视觉验收”仍必须由用户人工完成。

本批没有源码/生产代码修改，P14 RC 不刷新。P16 只新增本交接文档；测试专用 P15 提交 `1e5867e` 不进入 RC。

## RC 和运行边界

- ZIP：`C:\JTDX64\deps-webui\JTDX-2.2.159.2.10-rc-local-aef3434.zip`
- ZIP 大小：`56,242,139` bytes
- ZIP SHA256：`776df727a46242d65a1bdf1908fd86ac98019b39fb9935d7aadd1c28000e70cc`
- 本次实际运行目录：`C:\JTDX64\deps-webui\p14-rc-extract-2.2.159.2.10-aef3434-r1`
- 实际可执行文件：`C:\JTDX64\deps-webui\p14-rc-extract-2.2.159.2.10-aef3434-r1\bin\jtdx.exe`
- 权威日志：`C:\JTDX64\deps-webui\p16-rc-mainwindow-longrun-20260921-r2.log`
- 可复现 runner：`C:\JTDX64\deps-webui\p16_rc_mainwindow_longrun.ps1`

启动参数和环境：

```text
bin\jtdx.exe --test-mode --rig-name p16-rc-mainwindow
QT_QPA_PLATFORM=offscreen
```

DLL 搜索顺序以 RC `bin` 优先，其次为构建所需 MSYS2 MinGW、既有只读工具目录；没有启动 `rigctl`/`rigctld` 服务，没有连接串口、CAT、PTT、TX 或外部 UDP。

## 安全事实核对

这次没有只凭参数名称推断安全。源码事实如下：

- `main.cpp:176` 对 `--test-mode` 调用 `QStandardPaths::setTestModeEnabled(true)`；`main.cpp:202` 的锁路径和 `main.cpp:234` 的配置路径均由测试模式路径及独立应用名决定。
- `main.cpp:148-176` 支持独立 `--rig-name`；`main.cpp:214-230` 把实例名扩展为 `JTDX - p16-rc-mainwindow - test`，所以不会读取普通 JTDX 应用名的同名配置/锁。
- `Configuration.cpp:2654-2656` 的默认 Rig 来自 `TransceiverFactory::basic_transceiver_name_`，其定义为 `None`。
- `Configuration.cpp:2769` 默认 `AcceptUDPRequests=false`。
- `Configuration.cpp:2780-2801` 默认 Web UI 关闭、Web 自动端口虽为 true 但服务未启用、绑定 `127.0.0.1`、LAN/频率/DX/自动化控制均关闭。

本次进程采样只观察到 RC `jtdx.exe` 一个自有进程，没有 `jtdxjt9.exe` 子进程，也没有网络连接；这属于当前无音频/无设备测试实例的实际边界，不声称解码子进程或设备链路已验收。

## 持续运行结果

| 指标 | 结果 |
| --- | --- |
| 实际时长 | `1814.2` 秒，超过 30 分钟 |
| 采样 | 30 次，每约 60 秒 |
| 进程 | 始终为 1 个 RC `jtdx.exe`，无提前退出 |
| 私有内存 | `70.55–70.67 MB` |
| 工作集 | `22.41–22.57 MB` |
| 句柄 | `237–239` |
| 线程 | `1–4` |
| 网络连接 | `0–0` |
| 结束方式 | 受控终止自有 RC 路径 PID；正常 MainWindow 关闭未验证 |

该结果支持当前 `Rig=None`、Web 关闭、无设备、无网络测试配置下没有观察到明显的资源单调增长或异常退出；不外推数小时、生产运行、完整浏览器负载、音频解码、CAT/PTT/TX 或无线电行为。

## 最终 Release gate

| Gate | 状态 | 说明 |
| --- | --- | --- |
| RC ZIP 结构、75 文件、直根目录、SHA256、干净解压 | 已通过 | P14，SHA256 未变 |
| Qt/MinGW/FFTW/Hamlib 依赖闭包和无设备版本读取 | 已通过有界检查 | P14/P15；不等于真实设备 ABI/HIL |
| Web State/Control/Server 30 分钟隔离运行 | 已通过有界 fixture | P15，1802 秒、15 次恢复周期 |
| RC `jtdx.exe` 无硬件持续运行 | 已通过有界配置 | 本批 1814.2 秒、30 次采样、无网络 |
| RC 完整 MainWindow 视觉/设置 Tab/保存重载/菜单 | 待人工 | 当前工具不绕过原生 UI 限制 |
| RC 正常 MainWindow 退出和资源释放 | 待人工 | 本批受控终止，不能伪称正常关闭 |
| AutoSeq/CQ 实时解码和真实业务回读 | 待人工/设备 | AutoSeq 等待实时解码，不等于立即呼叫当前 DX |
| CAT、设备回读、PTT/TX、HIL | 未授权 | 需单独授权、设备和风险记录 |
| 公共 Release、上传、部署、正式 tag、最终批准 | NO-GO | 本批不执行 |

## 一次集中人工交接清单

### 无硬件优先

1. 在新的临时目录核对 ZIP SHA256，确认只使用 `bin/plugins/share`，不覆盖 `C:\JTDX64\159` 和普通用户配置。
2. 以唯一 `--test-mode --rig-name` 启动，人工在 Settings 中确认 `Rig=None`、UDP 请求关闭、Web UI 关闭、LAN 关闭及频率/DX/CQ/AutoSeq 控制分别关闭。
3. 保存并重新打开 Settings，确认上述安全默认值持久化；检查 Web 服务启动/停止、自动/手动端口、端口占用和菜单重复点击。
4. 使用程序支持的正常退出关闭 MainWindow，再次启动确认锁、端口和临时连接释放；若 Stop 已确认但普通 CQ/AutoSeq 仍显示未知锁，关闭并重启 JTDX，让新的 `JtdxWebControl` 实例恢复，不能刷新网页、删除日志或自动重发绕过。

### 真实设备/HIL（单独授权后）

1. 先记录设备型号、串口、CAT 配置、PTT 保护、TX 禁止条件和回滚方式；默认先只读 CAT/频率回读。
2. 单独验证 CAT 建链/断链/恢复、频率回读、PTT 状态和 TX 保护；不得把 HTTP `200`、`accepted`、软件 `pending` 或离线 fixture 当作设备完成。
3. AutoSeq 必须等待真实实时解码和业务状态回读；“选择当前 DX”与“立即呼叫当前 DX”不是同一动作，不能由 Web/UI 直接推断 TX。
4. 只有在独立 HIL 证据完整、用户明确批准后，才考虑发布批准；本 RC 审计不包含这些动作。

## 回退

回退范围仅为本批 RC ZIP、SHA256 清单、解压目录和本批日志；停止本批自有进程即可。不要修改用户安装、普通配置、UDP 监听、正式版本、tag 或远程仓库。当前 P16 不需要重新制作 RC。
