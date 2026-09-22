# P14 Release Candidate 本地审计（2026-09-21）

## 结论

本批生成了一个隔离的本地 Release Candidate（RC）运行包，未修改用户安装目录 `C:\JTDX64\159`，未打 tag、未上传、未部署、未连接真实电台，也未执行 CAT/PTT/TX/HIL。该包可作为后续人工 MainWindow、长时运行和 HIL 的独立候选，不可称为稳定公开 Release。

RC 源码基线为 `main/aef3434`（P13 最终资源构建证据提交）；候选包不引入新的源码修改。P20 后它是历史候选，不是当前包。历史 ZIP/SHA 清单位于 `C:\JTDX64\deps-webui\history\superseded`；原目录和解压目录已删除：

- 原目录：`p14-rc-2.2.159.2.10-aef3434`（已由 P20 删除可再生暂存目录）
- ZIP：`C:\JTDX64\deps-webui\history\superseded\JTDX-2.2.159.2.10-rc-local-aef3434.zip`
- SHA256 清单：`C:\JTDX64\deps-webui\history\superseded\JTDX-2.2.159.2.10-rc-local-aef3434-SHA256.txt`
- 原解压目录：`p14-rc-extract-2.2.159.2.10-aef3434-r1`（P20 已删除）

## 构建和依赖证据

| 项目 | 结果 |
| --- | --- |
| CMake 配置 | `Release`、`Ninja`、`JTDX_BUILD_LOCAL_TESTS=ON` |
| Qt | `C:\msys64\mingw64` 下的 Qt5 运行时及插件 |
| 编译/运行库 | MSYS2 MinGW GCC、FFTW、Qt5、pthread 运行库随包提供 |
| Hamlib | `hamlib-4.7.2` 头文件/库，运行包含 `msys-hamlib-4.dll` |
| OmniRig | `WSJT_ENABLE_OMNIRIG=OFF` |
| CMake 安装 | `C:\JTDX64\deps-webui\p14-install-20260921-r2.log`，退出码 0 |
| 最终构建 | `C:\JTDX64\deps-webui\p13-final-build-20260921.log` |
| 最终 UI 清洁构建 | `C:\JTDX64\deps-webui\p13-final-ui-clean-20260921.log` |
| 全量 CTest | `23/23`，`100% tests passed`；`C:\JTDX64\deps-webui\p13-final-ctest-dc62904-20260921.log` |

首次 CMake 安装只因旧构建缓存中的 `RIGCTL_EXE/RIGCTLD_EXE/RIGCTLCOM_EXE` 为 `NOTFOUND` 而停止；随后仅在独立构建缓存中补入既有 `C:\JTDX64\159\bin` 工具路径并重新安装，源码树未因此改变。该问题归类为打包环境缓存问题，不是源码测试失败。

对 RC 中所有 EXE/DLL 的导入审计只发现 Windows 系统 DLL（例如 `kernel32`、`user32`、`ws2_32`、`d3d11`）；未发现缺失的 Qt、MinGW、FFTW 或 Hamlib 第三方导入。CMake fixup 输出确认 7 个可执行文件已验证。

## 包结构和完整性

ZIP 必须直接包含以下三个根目录，不包含外层 staging 目录：

```text
bin/
plugins/
share/
```

复核结果：75 个文件，ZIP 文件项 75 个，另有 1 个目录项；直接根目录仅为 `bin`、`plugins`、`share`。SHA256 为：

```text
776df727a46242d65a1bdf1908fd86ac98019b39fb9935d7aadd1c28000e70cc
```

全新解压后的目录仍为 75 个文件；候选目录与解压目录逐文件 SHA256 比对为 0 个差异。包内包含 Qt 平台/音频/图像插件、Qt/MinGW/FFTW/Hamlib DLL、声音、`cty.dat`/`JPLEPH`、`ALLCALL7.TXT`/`CALL3.TXT` 和 `share/doc/JTDX` 下的许可及发行说明文件。

## 无硬件启动证据

候选目录和全新解压目录分别以以下边界启动：

```text
bin\jtdx.exe --test-mode --rig-name p14-rc-smoke
bin\jtdx.exe --test-mode --rig-name p14-rc-extract-smoke-r3
```

两次均使用 `QT_QPA_PLATFORM=offscreen`、候选包优先的 DLL 搜索路径和独立测试实例名。候选目录进程在 20 秒受控观察窗内持续运行；解压目录进程也在 20 秒观察窗内持续运行，随后只终止本次启动的候选路径进程。未观察到候选目录外的 JTDX 进程，也未操作用户实例。

源码默认值仍是安全边界：`Rig=None`、`AcceptUDPRequests=false`、Web UI 关闭、Web 自动端口开启但服务未启用、绑定 `127.0.0.1`、LAN 关闭、频率/DX/自动化控制均关闭；对应读取位置为 `Configuration.cpp:2654-2656`、`2769`、`2780-2801`。上述是源码/测试模式边界证据，不替代完整 MainWindow 的人工设置 Tab、保存重载和菜单验收。

## Release gate

| Gate | 当前结论 | 证据或剩余工作 |
| --- | --- | --- |
| Web UI 软件实现和状态机回归 | 已通过 | P13 最终代码及 `23/23` CTest |
| Release 构建、依赖闭包、CMake 安装 | 已通过 | P14 安装日志、导入审计、Qt/Hamlib/MinGW 运行文件 |
| ZIP 直根目录、逐文件完整性、干净解压 | 已通过 | 75 文件、3 个直根目录、SHA256、0 个解压差异 |
| 测试模式无硬件启动 | 已通过（有界） | 两个独立目录各持续观察 20 秒；不是长时稳定性证明 |
| 完整 MainWindow 桌面人工验收 | 待人工 | 设置 Tab、保存重载、菜单重复、退出释放、端口冲突/重启 |
| 无硬件长时运行和故障恢复 | 未验 | 需独立长时夹具/人工运行；当前短时启动不作稳定性结论 |
| 真实 CAT/PTT/TX、设备回读、无线电行为 | 未授权 | 需用户另行授权并单独记录 HIL 证据 |
| 最终版本/Release notes/发布批准 | 待最终负责人 | 当前只沿用仓库既有版本显示和本地 RC 标签，不发明正式版本号 |
| 公共 Release、上传、部署、tag | NO-GO | 本批明确不执行 |

## 后续人工清单

1. 将 ZIP 解压到新的临时目录，不覆盖 `C:\JTDX64\159` 和现有用户配置；先核对 SHA256 和三个直根目录。
2. 在无电台环境用独立 `--test-mode --rig-name` 启动，确认 `Rig=None`、UDP 请求关闭、Web UI 默认关闭；保存并重载配置，确认设置仍保持安全默认值。
3. 只在用户明确需要时开启 Web UI：确认 loopback/端口、频率/DX/CQ/AutoSeq 控制仍分别关闭；每个控制都必须看到业务状态回读，HTTP `200` 或 `accepted` 不算完成。
4. 检查菜单重复点击、服务停止/启动、端口占用、退出释放和重启；若出现未确认的 CQ/AutoSeq 锁，按现有安全说明关闭并重启 JTDX，不刷新网页、不删除日志、不自动重发绕过。
5. 进行独立的无硬件长时运行和故障恢复记录：启动/停止、服务重启、浏览器断开重连、日志轮转、异常退出和再次启动；记录时长、进程路径、配置实例名和结果。
6. 真实 CAT/PTT/TX/HIL 只有在另行授权后执行，并单独记录设备型号、端口、回读、PTT/TX 风险控制和可回滚步骤；本 RC 审计不替代该证据。

回滚只针对本批隔离目录、ZIP、SHA256 清单和解压目录；停止本批自有进程即可，不需要也不应修改用户安装、配置、UDP 或真实电台状态。
