# JTDX 内置 Web UI：进度与中断恢复日志

本文件是阶段交接记录。每次继续工作先更新“本次批次”表，再开始代码或测试；每次暂停前必须补齐命令和证据。不要把未来计划写成已完成事实。

## 当前恢复点

- 当前阶段：`P1 状态模型`。
- 本批状态：代码已提交，阶段验收未完成；`083b708` 已通过 Qt-only 局部证据，但完整配置/构建被 Hamlib SDK 缺失阻塞。
- 基线分支/提交：`main` / `04df89fc7be6066fb8b6ff243dece8ead7103dbb`。
- P0 初始结果提交：`02153b0`（提交后修订不 amend，使用 `git log --oneline -- docs/web-ui` 追踪后续文档提交）；提交后必须用 `git show --stat --oneline HEAD` 和 `git status --short --branch` 复核。
- 工作树预期：干净；任何后续未提交修改必须在恢复记录中列出。
- 模型策略：按用户要求使用 `gpt-5.6-luna`、`high`；不自行升级模型或推理档位。
- 配额策略：本批开始账户共享快照为 5 小时剩余 53%、周剩余 89%；结束快照为 5 小时剩余 17%、周剩余 84%，这些是账户共享读数，不是本任务独占额度。已低于 20%，暂停新批，只保留恢复记录。本任务不创建 5 小时定时任务或自动化监控。

## P1 本批记录

### 进行中检查点（先于代码）

- 开始额度：账户共享 5 小时窗口已用 47%（剩余 53%），周窗口已用 11%（剩余 89%）；低于 20% 前不开始新批。
- P1 目标：新增 `JtdxWebState.hpp/.cpp`，在主 Qt 事件循环线程维护只读状态快照；默认最多 300、硬上限 500 条解码；提供完整状态字段、未知值/null、单调新鲜度和递增 revision；复用主 `MessageClient` 状态/解码流并最小接入 `MainWindow`。
- 明确边界：不新增 TCP/UDP、线程、进程、控制、设置、菜单；不重复解析 UDP；不从 UI 控件反读字段；replay 解码不刷新实时新鲜度；应用在线与 rig 在线分开；名义/目标频率不冒充 CAT 实测频率。
- 计划文件：`JtdxWebState.hpp`、`JtdxWebState.cpp`、现有 `MainWindow` 接入点、P1 本地测试源/`CMakeLists.txt`（仅必要改动）、本文件及根入口同步事实。
- 验收：独立 `build-webui-dev` 配置/构建，原有 13 项 CTest 基线，P1 状态单测覆盖未知/刷新过期/上限淘汰/clear/revision/JSON 类型/实测频率与目标频率区分；不运行 `jtdx.exe`，不连接 CAT/PTT/TX，不做 HIL/部署。

### P1 构建前置探针（进行中）

- `C:\msys64\mingw64\bin\cmake.exe` 4.4.0、GCC/G++/GFortran 16.1.0、Ninja 均可用；MSYS2 bash 中显式加入 `/c/Windows/System32` 后，C/C++/Fortran 编译器识别和 ABI 探针通过。
- 独立目录 `C:\JTDX64\build-webui-dev-msys2` 已完成工具链探针，但配置在 `FindHamlib.cmake` 停止：`Hamlib_INCLUDE_DIR=<not found>`、`Hamlib_LIBRARY=<not found>`；定点递归检查 `C:\msys64` 未找到 `hamlib*.h`、`libhamlib*.a` 或 `*.dll.a`。
- 首次直接 PowerShell 配置还因生成器环境未发现 `mingw32-make`，第二次显式指定 make 后因系统 PATH 未传入 `chcp` 导致编译器检查失败；这两项已通过 MSYS2 bash + `/c/Windows/System32` 环境纠正，当前实质阻塞是 Hamlib 开发依赖缺失。
- 恢复命令（不下载/升级依赖）：`C:\msys64\usr\bin\bash.exe -lc 'export PATH=/mingw64/bin:/usr/bin:/c/Windows/System32:/c/Windows; cmake -G Ninja -S /c/JTDX64/jtdx_sourcecode -B /c/JTDX64/build-webui-dev-msys2 -DCMAKE_BUILD_TYPE=Release -DJTDX_BUILD_LOCAL_TESTS=ON -DWSJT_ENABLE_OMNIRIG=OFF -DCMAKE_C_COMPILER=/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=/mingw64/bin/g++.exe -DCMAKE_Fortran_COMPILER=/mingw64/bin/gfortran.exe -DCMAKE_MAKE_PROGRAM=/mingw64/bin/ninja.exe'`。

### P1 当前成果与限制

- 已实现：`JtdxWebState.hpp/.cpp`、主 `MessageClient` 本地观察信号、`MainWindow` 的状态/解码/clear/CAT 实测频率只读接入；不增加 UDP/TCP、线程、进程、控制、设置或菜单。UDP 目标为空或关闭时仍先更新本地状态；secondary mirror 不重复发布。
- 已覆盖：空状态未知/null、状态 fresh→stale、CAT offline 与 PTT 未确认、CAT stale、目标/实测频率分离、默认 300/硬 500 解码上限、最旧淘汰、clear/revision、JSON 类型、replay 不增长、不刷新、off-air 不刷新、实例 ID。off-air 仍可进入展示队列，但不刷新实时新鲜度且不可作为实时候选，未宣称不淘汰实时条目。
- 尚未接入的业务状态保留为 `null`：`auto_sequence_state`、`cq_state`、`current_tx_text`；FT8 解码 Call/Grid 仍保留原始 message，未新增解析器。P1 不将这些字段宣称为已完成业务接入。
- Qt-only 手工证据：`C:\JTDX64\build-webui-dev-msys2\jtdx_web_state_test.exe` 使用本机 Qt5 Core/MinGW 16.1.0 编译并运行通过（退出码 0）。完整复现命令和 `MessageClient.cpp` moc/语法检查命令已记录在本节末；完整配置、应用构建和原有 13 项 CTest 受 Hamlib 开发头/库缺失阻塞，未运行。
- 可复现 Qt-only 命令：`C:\msys64\usr\bin\bash.exe -lc 'export PATH=/mingw64/bin:/usr/bin:/c/Windows/System32:/c/Windows; g++ -std=gnu++14 -I/c/msys64/mingw64/include -I/c/msys64/mingw64/include/QtCore -I/c/JTDX64/jtdx_sourcecode /c/JTDX64/jtdx_sourcecode/JtdxWebState.cpp /c/JTDX64/jtdx_sourcecode/tests/jtdx_web_state_test.cpp /c/JTDX64/build-webui-dev-msys2/moc/moc_JtdxWebState.cpp -L/c/msys64/mingw64/lib -lQt5Core -o /c/JTDX64/build-webui-dev-msys2/jtdx_web_state_test.exe; /c/JTDX64/build-webui-dev-msys2/jtdx_web_state_test.exe'`，输出 exe 为 `C:\JTDX64\build-webui-dev-msys2\jtdx_web_state_test.exe`，退出码 0。MessageClient 语法检查使用 `moc.exe MessageClient.cpp -o C:\JTDX64\jtdx_sourcecode\MessageClient.moc` 后，以 `g++ -std=gnu++14 -fsyntax-only` 加入 `/c/JTDX64/build-webui-dev-msys2/moc`、QtCore/QtNetwork/QtGui/QtWidgets 和源码目录，结果通过；临时 moc 已删除。
- 阶段结论：P1 代码批次已提交但未验收完成。恢复后先恢复本机 Hamlib SDK，再执行 `build-webui-dev` 全构建、原有 13 项回归和 P1 集成合同测试；FT8 Call/Grid、`auto_sequence_state`、`cq_state`、`current_tx_text` 仍待接入并保留为 null。下一批还需补做 live decode 从 fresh 到 stale、`>UINT32_MAX` WSPR 频率边界、UDP 禁用/镜像观察端到端测试。
- 结束额度：5 小时窗口剩余 17%，周窗口剩余 84%；不再开启新批。

## P0 本批记录（历史记录；不覆盖当前 P1 状态）

### 已完成

- 阅读 `DEVELOPMENT_STATUS_zh-CN.md`、`CMakeLists.txt`、`README`、`main.cpp`、`mainwindow.*`、`MessageClient.*`、`MessageServer.*`、`Configuration.*` 和本地测试块。
- 确认当前仓库实际路径、HEAD、分支和工作树状态。
- 整理用户需求的网络/进程/线程/数据/控制边界、页面、设置、API、状态模型、测试分层和 HIL 禁令。
- 增补端口冲突、鉴权分级、Host/Origin/CSRF、连接限额、SSE 背压/重连、epoch/revision/幂等、过期解码、状态回读和停止优先级等改进项。
- 创建根入口及 `docs/web-ui/` 权威文档，并在 `DEVELOPMENT_STATUS_zh-CN.md` 加索引。
- 文件清单：`WEB_UI_START_HERE_zh-CN.md`、`docs/web-ui/01_需求与边界_zh-CN.md`、`docs/web-ui/02_设计_API_安全_zh-CN.md`、`docs/web-ui/03_阶段验收矩阵_zh-CN.md`、`docs/web-ui/PROGRESS_zh-CN.md`，以及现有状态文档的索引段落。
- 文档级检查：`git diff --check` 通过；未运行配置/编译/CTest/JTDX/浏览器/HIL/部署。

### 未完成

- 没有 `JtdxWebState`、`JtdxWebControl`、`JtdxWebServer` 或 Web 资源代码。
- 没有新增配置 Tab、菜单动作、API 路由、HTTP/SSE 实现或自动化测试。
- 没有配置/编译/运行 JTDX；没有进行浏览器、HIL、生产部署验证。
- 频率、DX、CQ/AutoSeq、停止的最终可复用业务入口和状态回读合同仍待 P1/P4/P5 源码审查。
- P3/P6 的本地浏览器验证仅可在隔离配置、`Rig=None`、已证明不会自动连接硬件且无真实 CAT 连接时进行；真实 CAT/PTT/TX/HIL 仍需单独授权。

### 下一步

P1 先确认 Qt/MinGW/CMake 的独立构建探针，不自动升级依赖；再只做状态模型和数据接入事实/最小实现：确定 `MessageClient`/MainWindow 状态信号的可复用边界、解码字段和新鲜阈值，增加 `JtdxWebState` 及单元测试。不开放控制，不监听 TCP，不新增设置或菜单。

## 恢复命令

```powershell
Set-Location C:\JTDX64\jtdx_sourcecode
git status --short --branch
git log -3 --oneline --decorate
git show --stat --oneline HEAD
rg -n "MessageClient|MessageServer|statusUpdate|status_update|decode|handle_transceiver_update|band_changed|process_Auto|AutoSeq|haltTx|on_stopTxButton_clicked|on_pbCallCQ_clicked" main.cpp mainwindow.cpp mainwindow.h MessageClient.cpp MessageClient.hpp MessageServer.cpp MessageServer.hpp Configuration.cpp Configuration.hpp
```

若恢复时发现 HEAD、分支、工作树或源码锚点改变，先把差异写入本文件，再重新核对，不覆盖他人修改。

## 分阶段交付规则

- 每个阶段一个本地中文 commit，提交前写明修改文件、测试命令、结果和未验证边界。
- 不使用 `git reset --hard`、`git checkout --` 覆盖文件；保留用户已有修改。
- 文档、静态检查、单元、API、浏览器、HIL、生产证据分开记录。
- 没有真实设备验证时，严禁声称已经验证 PTT、实际发射或无线电行为。
- HIL 和向群晖部署都需要独立明确授权。

## 后续批次日志模板

```text
### YYYY-MM-DD 阶段 Px 批次 N
- 开始额度：
- 基线（分支/HEAD/工作树）：
- 本批目标：
- 已完成：
- 未完成/阻塞：
- 修改文件：
- 命令与结果：
- 未运行：JTDX / CAT / PTT / TX / HIL / 部署（按实际填写）
- 下一步：
- 结束额度：
```
