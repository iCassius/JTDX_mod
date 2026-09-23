# JTDX 本地支持材料与清理记录

本目录用于存放源码工作树之外生成、但需要长期核查的本地证据和恢复索引。它不属于产品运行数据，也不应提交大型二进制、日志、截图、依赖包或构建产物。

## 归档布局

- `evidence/P26/`：P26 最终 Release 构建、CTest、安装日志，两个阶段的安装清单以及桌面/手机截图。共 11 个文件、366,275 bytes；从 `C:\JTDX64\deps-webui\evidence\P26` 逐文件复制并比对相对路径、长度和 SHA-256，11/11 一致。平台随后拒绝了包含删除操作的命令，故这只是归档副本，旧位置的 11 个原件仍在；不得称为迁移或清理完成。该目录由 `.gitignore` 排除；本 README、P26 报告和索引文档纳入源码版本控制。
- P26 其余行为、验证结论、证据边界与审阅包校验值见 `../docs/web-ui/P26-Web电台控制超时与六按钮收口_zh-CN.md`。
- Release ZIP、`.sha256` sidecar 和短说明仍留在 `C:\JTDX64` 根目录。当前 P26 为 `JTDX-2.2.159.2.10-local-b0732b9-P26.zip`；P25 和早期 P26 包保留作回退/历史比对。

## 构建与测试恢复

最终 P26 Release 在 `C:\JTDX64\build-webui-p25-release` 构建；这是 CMake `MinGW Makefiles` 生成树，缓存使用源码绝对路径 `C:\JTDX64\jtdx_sourcecode` 和 `C:\msys64\mingw64\bin\c++.exe`。此 build tree 有路径绑定，不要移动。按下面方式增量重建并运行全部测试：

```powershell
$env:PATH = 'C:\JTDX64\deps-webui\temp\P25-runtime-stage-05ee60d\bin;C:\msys64\mingw64\bin;C:\msys64\usr\bin;' + $env:PATH
& 'C:\msys64\mingw64\bin\cmake.exe' --build 'C:\JTDX64\build-webui-p25-release' --target all -- -j2
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:\msys64\mingw64\share\qt5\plugins\platforms'
& 'C:\msys64\mingw64\bin\ctest.exe' --test-dir 'C:\JTDX64\build-webui-p25-release' --output-on-failure
```

该缓存所依赖的工具链/材料保留在原位：MSYS2/MinGW 与 Qt 为 `C:\msys64\mingw64`，MSYS runtime 为 `C:\msys64\usr\bin`，Hamlib 源为 `C:\JTDX64\deps-webui\hamlib-4.7.2`，bundle/install 使用的 Hamlib SDK 为 `C:\JTDX64\deps-webui\temp\P21-hamlib-sdk`。P25 runtime stage 提供 Qt 测试运行 DLL，因此保留作 `PATH` 前缀。完整 Release 打包可以用上述构建树执行 `cmake --install <build> --prefix <新暂存目录>`；不要覆盖根目录已有 ZIP。

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
