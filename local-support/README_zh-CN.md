# JTDX 本地支持材料与清理记录

本目录位于源码工作树内，用于归档需长期核查的本地证据、恢复索引和分阶段候选材料。它不是产品运行数据；大型二进制、日志、截图、依赖包、生成树和解压材料通过 `.gitignore` 排除，不纳入 Git。

`dependencies/` 是本机依赖与来源材料的隔离副本，不属于源码提交：`source-archives/P22/` 保存源码/构建来源归档及其 SHA-256 清单，`hamlib-4.7.2/` 保存本机构建所需的 Hamlib 头文件和 import library。它们用于本地构建及来源追溯；保留原件，不纳入 Git。变更依赖前应先对照目录 README/清单核验来源和哈希。

## 后续目录规范与恢复入口

- 本 README 是本机恢复入口；打包入口为 `scripts/package-local-candidate.ps1`。从现在起，新构建树放 `build/<候选ID>/`，安装暂存树放 `staging/<候选ID>/`，独立解压放 `extract/<候选ID>/`，浏览器 profile 放 `browser-profiles/<候选ID>/`，日志/manifest 放 `evidence/<候选ID>/`（或 `logs/<候选ID>/`）。这些生成目录由 `.gitignore` 排除。
- 最终 ZIP 及 `.sha256` sidecar 仍按既定要求直接放在 `C:\JTDX64`；候选说明与长篇报告归档于 `releases/<阶段>/` 和 `docs/`，不得在根目录生成 staging、解压、日志、profile 或临时 build。
- 旧 build tree 包含绝对源码/工具链路径，不可直接 Move 后假装仍可用。本次权威 Release build `C:\JTDX64\build-webui-p25-release`、开发树 `C:\JTDX64\build-webui-dev-msys2` 和失败/待退役缓存 `C:\JTDX64\build-webui-p26-release` 均留原位；前两者仍供历史复核，P26 缓存待另行授权处理。以后新配置例：`cmake -S C:\JTDX64\jtdx_sourcecode -B C:\JTDX64\jtdx_sourcecode\local-support\build\P28-release`，使用独立生成树，不复制 CMakeCache。
- `deps-webui` 留在根目录原位：它包含有效工具链依赖、历史证据和此前被策略拒绝清理的项目；不得整目录搬迁，也不得通过改名、搬父目录或别的方式绕过既有拒绝。
- 打包脚本只从 `local-support/staging/<候选ID>` 读完整安装树；输出只会把 ZIP 和 sidecar 写到 `C:\JTDX64`，独立解压和证据均在 `local-support`。默认拒绝覆盖既有 ZIP、sidecar 或解压目录。运行前可用 `-ValidateOnly` 对既有 ZIP 做只读核验。

新候选的推荐执行顺序（示例 ID 为 P28；不复用旧 build tree）：

```powershell
$support = 'C:\JTDX64\jtdx_sourcecode\local-support'
$build = Join-Path $support 'build\P28-release'
$stage = Join-Path $support 'staging\P28'
$env:PATH = 'C:\JTDX64\deps-webui\temp\P25-runtime-stage-05ee60d\bin;C:\msys64\mingw64\bin;C:\msys64\usr\bin;' + $env:PATH
New-Item -ItemType Directory -Force -Path (Join-Path $support 'evidence\P28') | Out-Null
& 'C:\msys64\mingw64\bin\cmake.exe' -G 'MinGW Makefiles' -S 'C:\JTDX64\jtdx_sourcecode' -B $build -DCMAKE_BUILD_TYPE=Release -DJTDX_BUILD_LOCAL_TESTS=ON -DWSJT_ENABLE_OMNIRIG=OFF -DWSJT_HAMLIB_TRACE=OFF -DCMAKE_C_COMPILER='C:\msys64\mingw64\bin\cc.exe' -DCMAKE_CXX_COMPILER='C:\msys64\mingw64\bin\c++.exe' -DCMAKE_Fortran_COMPILER='C:\msys64\mingw64\bin\gfortran.exe' -DCMAKE_RC_COMPILER='C:\msys64\mingw64\bin\windres.exe' -DHamlib_INCLUDE_DIR='C:\JTDX64\deps-webui\hamlib-4.7.2\include' -DHamlib_LIBRARY='C:\JTDX64\deps-webui\temp\P21-hamlib-sdk\lib\libhamlib.dll.a' -DFFTW3_INCLUDE_DIR='C:\msys64\mingw64\include' -DFFTW3F_LIBRARY='C:\msys64\mingw64\lib\libfftw3f.dll.a' -DFFTW3F_THREADS_LIBRARY='C:\msys64\mingw64\lib\libfftw3f_threads.dll.a' -DRIGCTL_EXE='C:\JTDX64\deps-webui\temp\P21-runtime-stage-012da6f\bin\rigctl-jtdx.exe' -DRIGCTLCOM_EXE='C:\JTDX64\deps-webui\temp\P21-runtime-stage-012da6f\bin\rigctlcom-jtdx.exe' -DRIGCTLD_EXE='C:\JTDX64\deps-webui\temp\P21-runtime-stage-012da6f\bin\rigctld-jtdx.exe' *> (Join-Path $support 'evidence\P28\configure.log')
& 'C:\msys64\mingw64\bin\cmake.exe' --build $build --target all -- -j2 *> (Join-Path $support 'evidence\P28\final-build.log')
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:\msys64\mingw64\share\qt5\plugins\platforms'
& 'C:\msys64\mingw64\bin\ctest.exe' --test-dir $build --output-on-failure *> (Join-Path $support 'evidence\P28\final-ctest.log')
& 'C:\msys64\mingw64\bin\cmake.exe' --install $build --prefix $stage *> (Join-Path $support 'evidence\P28\install.log')
# Before packaging, place reviewed NOTICE files and third-party materials into $stage.
& (Join-Path $support 'scripts\package-local-candidate.ps1') -CandidateId 'P28' -PackageName 'JTDX-2.2.159.2.10-local-<commit>-P28'
```

检查每条命令的 `$LASTEXITCODE` 和日志再进入下一阶段。不要把旧 `deps-webui` 或根目录 build tree 当输出目录。打包脚本只从已审查、补齐 NOTICE/许可证材料的 stage 生成 ZIP、sidecar、manifest 和清洁解压校验；不会清理失败产物或覆盖文件。

新候选的推荐执行顺序（示例 ID 为 P28；不复用旧 build tree）：

```powershell
$support = 'C:\JTDX64\jtdx_sourcecode\local-support'
$build = Join-Path $support 'build\P28-release'
$stage = Join-Path $support 'staging\P28'
New-Item -ItemType Directory -Force -Path (Join-Path $support 'evidence\P28') | Out-Null
& 'C:\msys64\mingw64\bin\cmake.exe' -S 'C:\JTDX64\jtdx_sourcecode' -B $build -DCMAKE_BUILD_TYPE=Release -DJTDX_BUILD_LOCAL_TESTS=ON -DWSJT_ENABLE_OMNIRIG=OFF *> (Join-Path $support 'evidence\P28\configure.log')
& 'C:\msys64\mingw64\bin\cmake.exe' --build $build --target all -- -j2 *> (Join-Path $support 'evidence\P28\final-build.log')
& 'C:\msys64\mingw64\bin\ctest.exe' --test-dir $build --output-on-failure *> (Join-Path $support 'evidence\P28\final-ctest.log')
& 'C:\msys64\mingw64\bin\cmake.exe' --install $build --prefix $stage *> (Join-Path $support 'evidence\P28\install.log')
# Before packaging, place reviewed NOTICE files and third-party materials into $stage.
& (Join-Path $support 'scripts\package-local-candidate.ps1') -CandidateId 'P28' -PackageName 'JTDX-2.2.159.2.10-local-<commit>-P28'
```

检查每条命令的 `$LASTEXITCODE` 和日志再进入下一阶段。不要把旧 `deps-webui` 或根目录 build tree 当输出目录。打包脚本只从已审查、补齐 NOTICE/许可证材料的 stage 生成 ZIP、sidecar、manifest 和清洁解压校验；不会清理失败产物或覆盖文件。

## P030 当前源码与软件验证结果（未打包）

- 源码提交 `6283cac280f568c8681c5aa032157c45c63877c0`；显示版本 `2.2.159.030`，Windows PE 数字版本 `2.2.159.30`；完整 Release 构建成功，全量 CTest 30/30（57.47 秒）。本批没有生成 ZIP、安装 stage 或清洁解压。
- 新/无 `WebUiBindAddress` 配置默认 `0.0.0.0`；现存配置值原样保留，尤其不把旧版本已持久化的 `127.0.0.1` 静默改为 LAN 监听。Windows URL 按默认路由与接口综合 metric 选择有效 IPv4，无可用地址时回退 `127.0.0.1`；具体实现和验收边界见 [`releases/P030/说明_zh-CN.md`](releases/P030/说明_zh-CN.md)。
- 最终日志在 `evidence/P030/final-build.log` 与 `evidence/P030/final-ctest.log`。没有运行 P030 主程序、真实 CAT/PTT/TX、LAN/浏览器端到端或 HIL；未改 UDP、系统路由或防火墙。

## P029 历史本机审阅候选

- 版本 `2.2.159.029`，源码提交 `ec26de60fb55ed0044a014fa5e6fdb2b0457d4b4`；Windows PE 数字版本 `2.2.159.29`，构建标题时间 `2026-09-24 15:12:04 UTC`。Release 全量构建成功，全量 CTest 29/29；完整说明见 [`releases/P029/说明_zh-CN.md`](releases/P029/说明_zh-CN.md)。
- 当前本机候选 ZIP：`C:\JTDX64\JTDX-2.2.159.029-local-ec26de6-P029.zip`；SHA-256 `E624A05A404721FF074B9112C04C2C16376605C47B0972D6F4BD6F7C84743D98`，49,381,555 bytes、142 个文件、manifest 总内容 137,547,744 bytes；暂存↔ZIP↔清洁解压逐项哈希/大小 142/142 一致，ValidateOnly 通过。P028 ZIP/sidecar 未覆盖。
- 频谱窗关闭后模式切换/复位保持隐藏，显隐意图跨重启保存，菜单显式打开可恢复显示；右侧 Rx Frequency 原有过滤业务未改。用户验收步骤和 Qt/Windows 焦点验证边界见候选说明及 [`../docs/widegraph-visibility-and-rx-window_zh-CN.md`](../docs/widegraph-visibility-and-rx-window_zh-CN.md)。
- 运行库/插件/授权材料从已审阅 P028 stage 逐文件核对后补齐。CMake install 的 `fixup_bundle` 缺少 `objdump`，不把其依赖扫描当作通过；静态暂存审计见 `evidence/P029/stage-audit.log`。未启动真实 JTDX、未操作 CAT/PTT/TX，未做 MainWindow 焦点、HIL、网络部署或法律清权验证；仅供本机审阅，不公开分发。

## P028 历史本机审阅候选

- 新候选包：`C:\JTDX64\JTDX-2.2.159.028-local-bc45bb4-P028.zip`，SHA-256 `2771BCB2CB898ECC39A1D1B6796579EF220E14CD90D2B5D9534019FA81715F89`；142 文件、49,380,789 bytes。暂存、ZIP、独立解压均逐项 142/142 同长度/哈希，ValidateOnly 通过。
- 当前源码提交 `bc45bb4d54bbb5729513e5d7763e8f681e320af3`；应用显示版本 `2.2.159.028`，PE 数字版本 `2.2.159.28`；主窗口构建时间 `2026-09-24 12:50:19 Asia/Shanghai (UTC+08:00)`。下一次手动发布版本应改为 `.029`，不自动递增。
- 根因已修：SSE 服务在 298,124-byte 合法快照仍有待发数据时，误把 65,652-byte 的瞬时 Qt 发送队列认作慢读并提前断开；该快照加待发内容仅约 298KB，低于 1MiB 上限。移除这个 16KiB 瞬时判定，保留单事件/总队列上限和 2 秒 drain deadline。针对性 SSE 测试通过；修复后连续两轮全量 CTest 均 28/28。浏览器夹具 7/7。
- 旧失败候选 `JTDX-2.2.159.028-local-c545085-P028.zip` 及 sidecar 保留原样，SHA-256 `1D3D517EA704F3047F300A8BBB03C95FE41A3393C4468BC3C502F82A2656A13D`，状态为 **SUPERSEDED**；标记见 [`releases/P028/c545085-SUPERSEDED_zh-CN.md`](releases/P028/c545085-SUPERSEDED_zh-CN.md)。诊断、构建和连续测试证据见 `evidence/P028-final-bc45bb4/`。
- 仅供本机审阅，不公开分发。未启动候选程序或操作 CAT/PTT/TX，未做硬件/HIL、真实 MainWindow 端到端、LAN/公网或部署验证；Qt/LGPL、间接依赖和数据文件再分发授权仍待审计。完整说明见 [`releases/P028/说明_zh-CN.md`](releases/P028/说明_zh-CN.md)。
- 仅供本机审阅，不公开分发。未启动候选程序或操作 CAT/PTT/TX，未做硬件/HIL、真实 MainWindow 端到端、LAN/公网或部署验证；Qt/LGPL、间接依赖和数据文件的再分发授权仍待审计。完整记录见 [`releases/P028/说明_zh-CN.md`](releases/P028/说明_zh-CN.md)。

## P28 历史本机审阅候选

- 候选包：`C:\JTDX64\JTDX-2.2.159.2.10-local-c196a09-P28.zip`，SHA-256 `8F95E9F79C993650A703D63C225830E6669891186FAD05BC5ED99D9671337D37`；142 文件、49,380,321 bytes，直接根项为 `bin/`、`plugins/`、`share/`、`NOTICE`、`NOTICE_zh-CN.md`。ZIP/sidecar 在工作区根目录；独立暂存和解压、manifest、安装及校验日志都在 `local-support`。
- 源码提交 `c196a091053eabd1a330a5942c8f2c8a953dedc6`，父提交为 `302b062`；P27 的 `3c086af` 是其历史祖先。Release 构建/CTest 复用源码内有效 Release build：`evidence/dependency-audit/live-state-sync-root-review-final-build.log`、`live-state-sync-root-review-final-ctest.log`（27/27，56.94 秒）。
- 运行依赖来自当前 MSYS2 Qt 和已哈希核验的 Hamlib 4.7.2；只复用单个 P27 Hamlib DLL，不复用 P25 Qt runtime。依赖审计见 `evidence/P28/runtime-provenance.log`、`qt-runtime-manifest.csv`、`P27-P28-tree-comparison.csv` 与 `install.log`；包完整性见 `evidence/P28/manifest-P28.csv`、`package-verify.log`、`package-validate-only.log`。
- 状态低延迟路径：MainWindow 20ms 合并采集 + Server 20ms 合并 SSE 推送；1 秒周期 revision 检查、2 秒快照兜底、10 秒 heartbeat。浏览器数值来自模拟 HTTP/SSE；首轮提交后有一次夹具等待超时，重跑 6/6 通过，运行记录见 `evidence/P28/browser-fixture-run-history.log`，不能声称真实 MainWindow 端到端或硬件已验证。
- 仅供本机审阅，不公开分发。不要覆盖或并行运行已有 JTDX；保留 P27 作回退。Qt/LGPL 与间接依赖完整清权、数据文件再分发授权，以及 MainWindow/HIL/部署仍未完成。详见 [`releases/P28/说明_zh-CN.md`](releases/P28/说明_zh-CN.md)。

## P27 历史本机审阅候选

- 候选包：`C:\JTDX64\JTDX-2.2.159.2.10-local-3c086af-P27.zip`，SHA-256 `9B0A3AC0D537982320F1F1566439ABC7116DAE9F4B41E8A06D56A181C7B02DD8`；142 文件、49,376,951 bytes，根项 `bin/`、`plugins/`、`share/`、`NOTICE`、`NOTICE_zh-CN.md`。独立解压与 manifest 142/142 文件大小和哈希相同。
- 源码提交 `3c086af9aa6ff6f9ac57513277740cda5145683e`，包含 P26 Web UI 以及 AutoSeq CQ 冷却来源与筛选覆盖修复。P26 `b0732b9` 包不包含此 AutoSeq 修复。
- Release 构建/安装和全量 CTest 27/27 证据见 `evidence/P27/`；完整包说明见 `../docs/web-ui/P27-AutoSeq修复本机审阅包_zh-CN.md`。暂存树在 `staging/P27-runtime-stage-3c086af`，清洁解压在 `extract/P27-clean-extract-3c086af`；两者均已从 `deps-webui/temp` 归整并核对 142/142 文件哈希，不涉及 P25/P26 被限制的临时项。
- “大量 X”显示现象未在最近 `202609_ALL.TXT` 复现；本批不据此改解码器。现场样例和 WAV/截图仍是进一步定因所需证据。
- 未启动候选程序或连接/操作 CAT/PTT/TX，未发射，未做 HIL、LAN/公网或部署验收；候选包不代表公开发行批准或第三方清权。

## 归档布局

- `releases/P25/`：P25 简要说明与清洁解压候选（141 文件、137,514,589 bytes），从根目录移入；内容逐相对路径、长度、SHA-256 复核一致。这里只是归档位置变更，不释放磁盘空间。
- `releases/P26/`：两份 P26 根目录简要说明原位归档；对应 ZIP/sidecar 仍留在 `C:\JTDX64` 根目录。
- `releases/P27/`：P27 根目录简要说明原位归档；ZIP 与 sidecar 仍留在 `C:\JTDX64` 根目录。
- `releases/P28/`：P28 中文审阅说明；ZIP 与 sidecar 留在 `C:\JTDX64` 根目录。
- `releases/P028/`：版本 `2.2.159.028` 本机审阅说明；独立 ZIP 与 sidecar 留在 `C:\JTDX64` 根目录，旧 P28 ZIP/sidecar 保持不变。
- `releases/P028/c545085-SUPERSEDED_zh-CN.md`：旧失败候选保留标记；旧 ZIP/sidecar 未覆盖。
- `staging/P028-final-bc45bb4/` 和 `extract/P028-final-bc45bb4/`：最终 P028 的独立暂存及清洁解压树，142/142 文件 SHA-256 一致；证据位于 `evidence/P028-final-bc45bb4/`。旧暂存/解压 `P028-version028` 保留作为复核证据。
- `staging/P28/` 和 `extract/P28/`：P28 复用当前 Release build 安装到隔离暂存树、再由受约束脚本独立解压；与 ZIP 142/142 文件大小及 SHA-256 一致。
- `staging/P27-runtime-stage-3c086af/` 和 `extract/P27-clean-extract-3c086af/`：本轮新建且可重建的 P27 安装暂存/清洁解压，已从 `deps-webui/temp` 搬入源码支持目录；各 142 文件、137,520,019 bytes，逐项 SHA-256 一致。
- `scripts/package-local-candidate.ps1`：新的受路径约束打包/校验入口；未来 build、stage、extract、profile 与日志均须放在 `local-support` 子目录。

- `evidence/P26/`：P26 最终 Release 构建、CTest、安装日志，两个阶段的安装清单以及桌面/手机截图。共 11 个文件、366,275 bytes；从 `C:\JTDX64\deps-webui\evidence\P26` 逐文件复制并比对相对路径、长度和 SHA-256，11/11 一致。平台随后拒绝了包含删除操作的命令，故这只是归档副本，旧位置的 11 个原件仍在；不得称为迁移或清理完成。该目录由 `.gitignore` 排除；本 README、P26 报告和索引文档纳入源码版本控制。
- P26 其余行为、验证结论、证据边界与审阅包校验值见 `../docs/web-ui/P26-Web电台控制超时与六按钮收口_zh-CN.md`。
- Release ZIP 与 `.sha256` sidecar 留在 `C:\JTDX64` 根目录。当前候选见上方 P028；P28/P27、P26、P25 和早期 P26 包保留作回退/历史比对。

## 构建与测试恢复

最终 P26 Release 在 `C:\JTDX64\build-webui-p25-release` 构建；这是 CMake `MinGW Makefiles` 生成树，缓存使用源码绝对路径 `C:\JTDX64\jtdx_sourcecode` 和 `C:\msys64\mingw64\bin\c++.exe`。此 build tree 有路径绑定，不要移动。按下面方式增量重建并运行全部测试：

```powershell
$env:PATH = 'C:\JTDX64\deps-webui\temp\P25-runtime-stage-05ee60d\bin;C:\msys64\mingw64\bin;C:\msys64\usr\bin;' + $env:PATH
& 'C:\msys64\mingw64\bin\cmake.exe' --build 'C:\JTDX64\build-webui-p25-release' --target all -- -j2
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:\msys64\mingw64\share\qt5\plugins\platforms'
& 'C:\msys64\mingw64\bin\ctest.exe' --test-dir 'C:\JTDX64\build-webui-p25-release' --output-on-failure
```

该缓存所依赖的工具链/材料保留在原位：MSYS2/MinGW 与 Qt 为 `C:\msys64\mingw64`，MSYS runtime 为 `C:\msys64\usr\bin`，Hamlib 源为 `C:\JTDX64\deps-webui\hamlib-4.7.2`，bundle/install 使用的 Hamlib SDK 为 `C:\JTDX64\deps-webui\temp\P21-hamlib-sdk`。P25 runtime stage `C:\JTDX64\deps-webui\temp\P25-runtime-stage-05ee60d` 提供 Qt 测试运行 DLL，是有效依赖，保留作 `PATH` 前缀；它不是未来输出路径。P25 清洁解压候选现归档在 `local-support/releases/P25/clean-extract-05ee60d`。新 Release 安装前缀必须在 `local-support/staging/<候选ID>`；不要覆盖根目录已有 ZIP。

`C:\JTDX64\build-webui-dev-msys2` 是另一棵既有本地开发构建树（Release/MinGW，约 298.8 MB），与 P25 权威验证树分别保留；它不是 P26 最终测试证据，也未在本次重跑。失败的 `build-webui-p26-release` 仅含配置缓存/生成文件，未作为证据使用；本次拟清理但命令被平台策略拒绝，目前仍在原位。若要建立全新构建树，应按源码文档与依赖配置重新配置，不要复制或搬运 CMakeCache/生成树。

复用既有开发树时，构建命令为：

```powershell
$env:PATH = 'C:\JTDX64\deps-webui\temp\P25-runtime-stage-05ee60d\bin;C:\msys64\mingw64\bin;C:\msys64\usr\bin;' + $env:PATH
& 'C:\msys64\mingw64\bin\cmake.exe' --build 'C:\JTDX64\build-webui-dev-msys2' --target all -- -j2
```

两树 CMake generator/compiler/build-type 相同，但分别保留开发构建状态和最终 P26 验证输出（测试二进制清单/大小已有差异）。开发树不是此次验证所必需；因用途不能仅凭相同 cache 设置判为可删副本，本批保留，不把两树互相覆盖。

## 临时项与历史材料清单

### 本批拟清理但未移除的 P26 目标

10 个精确目录被放在同一条原生 PowerShell 清理命令中；该命令在进程启动前被平台以 `CreateProcess Rejected` / `blocked by policy` 拒绝。命令未执行，平台没有返回针对单个路径的原因，不能推断哪一项目标触发拒绝；未拆分命令、未换 shell/工具重试。下表所有路径目前仍存在，零删除、零释放。此前盘点已检查候选目录大小、reparse point 状态、进程命令行和 P25/P26 包 sidecar；因拒绝发生在进程启动前，该命令内的最终 `Resolve-Path` 与删除前复核没有执行。P26 截图、日志及 manifest 已先归档并逐项 SHA-256 验证，但旧证据原件也仍在。

| 路径 | 清理前大小 | 用途/可重建方式 |
|---|---:|---|
| `deps-webui/temp/P26-cdp-mobile-profile-5145c54` | 127,548,233 bytes | 一次性 CDP 手机视口测试浏览器用户配置；可重建；仍在 |
| `deps-webui/temp/P26-cdp-verify-5145c54` | 127,549,641 bytes | 一次性 CDP 视口测量配置；可重建；仍在 |
| `deps-webui/temp/P26-chrome-capture-profile-5145c54` | 6,872,305 bytes | 旧的一次性截图 profile；可重建；仍在 |
| `deps-webui/temp/P26-chrome-capture-profile-5ff2d47` | 7,958,456 bytes | 旧的一次性截图 profile；可重建；仍在 |
| `deps-webui/temp/P26-clean-extract-a1eaf35-842bfea` | 137,515,613 bytes | 早期 P26 清洁解压快照；仍在。与 `P26-clean-extract-a1eaf35-final` 的 3 个程序文件 SHA-256 不同（`bin/jtdx.exe`、`bin/udp_daemon_jtdx.exe`、`bin/wsprd_jtdx.exe`；长度相同但内容不同），构建来源未映射前不能视为重复/可丢弃 |
| `deps-webui/temp/P26-clean-extract-a1eaf35-final` | 137,515,613 bytes | 另一早期 P26 清洁解压快照；仍在。三项二进制差异见上一行；保留来源区分，暂不宣称可从同一 ZIP 重建 |
| `deps-webui/temp/P26-clean-extract-b0732b9` | 137,517,529 bytes | 当前包验证解压副本；ZIP、sidecar、manifest 保留，可重新解压；仍在 |
| `deps-webui/temp/P26-runtime-stage-a1eaf35` | 137,517,529 bytes | 已打包的安装暂存副本；可由保留的 Release build 重新 `cmake --install`；仍在 |
| `build-webui-p26-release` | 391,850 bytes | 未用于最终验证的配置缓存/残留生成文件；可由 CMake 重新配置；仍在 |
| `deps-webui/evidence/P26` | 366,275 bytes | 11 份原证据；已复制到本地支持目录，hash 一致；原目录仍在 |

### 本批保留的 P21–P25 临时目录

下列历史临时材料仍位于 `C:\JTDX64\deps-webui\temp`。它们分别用于 Hamlib 源、SDK、安装、运行时和各阶段 Release ZIP/清洁解压验收；部分被旧任务记录或依赖链引用。遵守既有清理限制，本批不尝试删除或改名，不通过其他 shell/工具绕过限制。材料原则上可从保留源码/压缩包和阶段 ZIP 重建，但重建涉及旧工具链与历史核验，应另行盘点授权后再做。

| 目录 | 清理前约大小 | 用途 |
|---|---:|---|
| `P21-clean-extract-012da6f` | 131.20 MiB | P21 候选包清洁解压核验 |
| `P21-hamlib-official-extract` | 16.60 MiB | 官方 Hamlib 源包展开内容 |
| `P21-hamlib-sdk` | 13.30 MiB | Hamlib bin/lib SDK；安装打包依赖 |
| `P21-hamlib-source` | 15.20 MiB | Hamlib 源码副本 |
| `P21-install-012da6f` | 42.70 MiB | P21 独立安装前缀 |
| `P21-runtime-stage-012da6f` | 131.20 MiB | P21 runtime bundle |
| `P22-runtime-stage-50ad747` | 39.90 MiB | P22 早期/不完整 runtime 暂存 |
| `P22-runtime-stage-50ad747-r1` | 131.20 MiB | P22 候选 runtime bundle |
| `P22-test-qt-platform` | 0.10 MiB | P22 offscreen Qt platform 测试材料 |
| `P23-clean-extract-70be7d` / `P23-runtime-stage-00cba47` | 各 131.10 MiB | P23 包核验解压副本及 runtime stage |
| `P24-clean-extract-6dbb210` / `P24-runtime-stage-6dbb210` | 各 131.10 MiB | P24 包核验解压副本及 runtime stage |
| `P25-clean-extract-05ee60d` / `P25-runtime-stage-05ee60d` | 各 131.10 MiB | P25 回退 ZIP 核验副本及当前测试 PATH/runtime stage |

`temp` 根部的 `P21-hamlib-w64-4.7.2.zip`（3,228,937 bytes）和 `P21-qt5-base-5.15.19-kde-r96-1.src.tar.zst`（323,565,642 bytes）是依赖/源码材料，保留；不把依赖源或有效 SDK 当作垃圾清理。

### 历史证据、依赖与候选包（均保留）

- `deps-webui/evidence/history`、`P19`、`P20`、`P22`、`P25`：历史阶段的构建、CTest、许可证或功能证据；旧文档引用仍有效。分别为 3/3/6/21/21 个文件，合计约 2.5 MiB。
- `deps-webui/history/rollback`（2 个旧回退 ZIP 与 sidecar）保留 P17/P18；`history/superseded`（6 个旧 ZIP、sidecar 与说明）保留 P14/P19/P20/P21，全部属于历史候选而非当前包。两目录共 13 个文件、约 315.40 MiB；保留原因是可回退及旧审阅链，不能重建成完全相同历史字节。
- `deps-webui/release-current`（11 文件、约 141.31 MiB）含 P22 ZIP/sidecar、P23/P24 ZIP/sidecar、P24/P25 manifests 与说明；对应历史候选及归档核验，重建需各自旧源码/工具链，保留。
- `deps-webui/source-materials/P22`（10 文件、约 436.94 MiB）含 JTDX P22 源码 ZIP、Hamlib、FFTW、GCC、Qt5 base/multimedia/serialport/websockets 源码归档及 SHA256/README；这些是来源/许可材料，重新下载可能变化，保留。
- `deps-webui/hamlib-4.7.2`（1,043 文件、约 15.21 MiB）为 Hamlib 4.7.2 源码；`deps-webui/generated/msys-hamlib-4.def`（11,083 bytes）为本地生成定义文件；均用于依赖来源/构建记录，保留。
- `deps-webui/evidence/history`（P18）、`P19`（LogQSO/构建）、`P20`（clean-first/CTest/许可审计）、`P22`（候选包与依赖审计）、`P25`（解码和安全门/安装证据）都保留，供对应旧报告核验；目录大小/文件数已在本 README 上方索引汇总，不能替代其所记录的历史环境。
- `deps-webui` 根级保留 182 个阶段 `.log`（497,880 bytes）、`INDEX_zh-CN.md`（3,017 bytes）、Hamlib import library（477,782 bytes）、root Hamlib definition（165 bytes）、`p15_web_fixture_longrun.ps1`（14,748 bytes）、`p16_rc_mainwindow_longrun.ps1`（4,736 bytes）、`hamlib-4.7.2.tar.gz`（3,107,379 bytes）及两个空 P3 fixture 输出；日志和脚本是历史构建/测试记录，不可从当前源码还原原始运行输出，故保留。注意 temp 根另有 3,228,937-byte Hamlib ZIP 与 323,565,642-byte Qt5 source archive，见上表。
- `C:\JTDX64\159`、`159.bak`、P25 解压候选目录、所有根级 P25/P26 ZIP 和 sidecar 均未改动；当前 P26 与 P25 回退包哈希需继续匹配原记录。

## 清理结果与限制

本批实际释放 0 bytes、删除 0 项。P26 临时目录、失败 build 缓存及 P26 旧 evidence 原件仍保留；P21–P25 临时目录、历史 evidence/deps、两棵既有开发/验证构建树也均保留；这不是全量清空。用户要求的 Explorer 手动候选说明位于 `C:\JTDX64\待手动清理\清理候选清单_2026-09-24.md`；该文件夹仅含说明，不含移动来的文件。CMake build tree 有绝对路径，不能直接移动。没有启动真实 JTDX、CAT/PTT/TX/HIL，也没有重建无关项目。
