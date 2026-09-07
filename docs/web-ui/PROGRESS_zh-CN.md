# JTDX 内置 Web UI：进度与中断恢复日志

本文件是阶段交接记录。每次继续工作先更新“本次批次”表，再开始代码或测试；每次暂停前必须补齐命令和证据。不要把未来计划写成已完成事实。

## 当前恢复点

- 当前阶段：`P1 状态模型`。
- 本批状态：已完成（源码结果提交 `0420a21`，文档结果提交 `b788d0e`）；已确认上一批代码提交为 `083b708`，上一轮阻塞记录为 `1f5b71f`。本批补齐 P1 边界测试、只读状态接入并恢复匹配 Hamlib 开发头/导入库。
- 基线分支/提交：`main` / `1f5b71fe6b65e039cc8df6c4ebbd2477ec4f73e1`（2026-09-08 批次开始实际读数）。
- P0 初始结果提交：`02153b0`（提交后修订不 amend，使用 `git log --oneline -- docs/web-ui` 追踪后续文档提交）；提交后必须用 `git show --stat --oneline HEAD` 和 `git status --short --branch` 复核。
- 工作树预期：本批结束前列出并精确提交本批修改；不得覆盖其他工作。
- 模型策略：按用户要求使用 `gpt-5.6-luna`、`high`；不自行升级模型或推理档位。
- 配额策略：本批开始账户共享快照为 5 小时剩余 93%、周剩余 80%；本批收尾快照（2026-09-08）为 5 小时剩余 26%、周剩余 70%。这些是账户共享读数，不是本任务独占额度；当前仍高于新批阈值 20%，但本批不再扩展范围，只收尾并写恢复记录。本任务不创建定时任务或自动化监控。

## P1 本批记录

### 2026-09-08 P1 批次（已完成；以下先记录开始检查点）

- 开始额度：账户共享 5 小时窗口剩余 `93%`（used `7%`），周窗口剩余 `80%`（used `20%`）；低于 `20%` 时停止新批，只收尾并写恢复点。额度读数来自本批开始的实时查询。
- 本批目标：在既有 `JtdxWebState`/`MessageClient`/`MainWindow` 只读接入上补足有界 P1：复用现有 `DecodedText` 的 Call/Grid 事实，补 `auto_sequence_state`、`cq_state`、`current_tx_text` 的内部业务字段快照，保持单调新鲜度、应用在线与 CAT 在线分离且无控制副作用；增加 live decode fresh→stale、WSPR `>UINT32_MAX`、UDP 空/disabled 仍观察且 mirror 不重复、clear/状态 JSON 合同测试。
- 明确边界：不做 P2 TCP server/API/SSE，不新增 UDP 监听、线程、进程、控制、设置、菜单；不重复解析 UDP，不从控件反读业务状态；不启动 `jtdx.exe`，不连接 CAT/PTT/TX，不做 HIL 或部署；允许运行既有 loopback UDP 测试夹具，但它不是生产 Web 新监听。
- 文件计划（先事实后最小修改）：`JtdxWebState.hpp/.cpp`、现有 `MessageClient.*`/`mainwindow.*` 只读接入点、P1 测试源、必要文档；Hamlib 只在独立 `deps-webui`/`build-webui-dev-msys2` 使用，不全局安装/升级、不替换现用运行库。
- 验收计划：Qt-only P1 单测与既有 UDP loopback 夹具先行；恢复匹配 Hamlib 后独立配置/构建和完整 CTest。严格区分静态检查、Qt-only 单测、完整构建/CTest、应用/HIL/部署；完整配置失败时不得运行旧 exe 掩盖失败。

### P1 构建前置探针与恢复结果（已完成）

- `C:\msys64\mingw64\bin\cmake.exe` 4.4.0、GCC/G++/GFortran 16.1.0、Ninja 均可用；MSYS2 bash 中显式加入 `/c/Windows/System32` 后，C/C++/Fortran 编译器识别和 ABI 探针通过。
- `C:\msys64\mingw64\bin\cmake.exe` 4.4.0、GCC/G++/GFortran 16.1.0、Ninja 均可用；MSYS2 bash 中显式加入 `/c/Windows/System32` 后，C/C++/Fortran 编译器识别和 ABI 探针通过。
- 官方 Hamlib 4.7.2 release 资产 `C:\JTDX64\deps-webui\hamlib-4.7.2.tar.gz` SHA256=`AE1FCF2DBC80EA0786EA8F047B09399C3F7737D1930442F61A031708ED33E88F`；现用 `C:\JTDX64\159\bin\msys-hamlib-4.dll` SHA256=`630A90E02F56E0D5F02A77E8D172F61F041399900DBFFA57A4FB989896895DB3`。从该现用 DLL 生成独立导入库 `C:\JTDX64\deps-webui\libhamlib-4.7.2-from-existing-dll.a`，没有重新编译或替换现用 DLL。
- 4.7.2 头文件静态 ABI 探针 `C:\JTDX64\deps-webui\abi_probe.cpp` 通过：`sizeof(rig_state)=31416`、`offsetof(tx_vfo)=13364`、`offsetof(vfo_list)=13312`、`offsetof(pttport)=14840`、`sizeof(s_rig)=47752`、`sizeof(rig_caps)=26072`。
- 独立 CMakeCache 已读回 `Hamlib_INCLUDE_DIR=C:/JTDX64/deps-webui/hamlib-4.7.2/include`、`Hamlib_LIBRARY=C:/JTDX64/deps-webui/libhamlib-4.7.2-from-existing-dll.a`、`WSJT_ENABLE_OMNIRIG=OFF`、`WSJT_GENERATE_DOCS=OFF`；首次配置仅因缺 `asciidoctor` 停止，关闭文档生成后配置成功并显示 `Found Hamlib`。
- 配置命令：`C:\msys64\usr\bin\bash.exe -lc 'export PATH=/mingw64/bin:/usr/bin:/c/Windows/System32:/c/Windows; cmake -G Ninja -S /c/JTDX64/jtdx_sourcecode -B /c/JTDX64/build-webui-dev-msys2 -DCMAKE_BUILD_TYPE=Release -DJTDX_BUILD_LOCAL_TESTS=ON -DWSJT_ENABLE_OMNIRIG=OFF -DWSJT_GENERATE_DOCS=OFF -DCMAKE_C_COMPILER=/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=/mingw64/bin/g++.exe -DCMAKE_Fortran_COMPILER=/mingw64/bin/gfortran.exe -DCMAKE_MAKE_PROGRAM=/mingw64/bin/ninja.exe -DHamlib_INCLUDE_DIR=/c/JTDX64/deps-webui/hamlib-4.7.2/include -DHamlib_LIBRARY=/c/JTDX64/deps-webui/libhamlib-4.7.2-from-existing-dll.a'`；日志：`C:\JTDX64\deps-webui\configure-webui.log`。

### P1 当前成果与限制

- 已实现：`JtdxWebState.hpp/.cpp`、主 `MessageClient` 本地观察信号、`MainWindow` 的状态/解码/clear/CAT 实测频率只读接入；不增加 UDP/TCP、线程、进程、控制、设置或菜单。UDP 目标为空或关闭时仍先发出本地观察；secondary mirror 不重复发送相同目标的结构化包。
- 已覆盖：空状态未知/null、状态和 live decode fresh→stale、CAT offline 与 PTT 未确认、目标/实测频率分离、默认 300/硬 500 解码上限、最旧淘汰、clear/revision、JSON 类型、replay 不增长、不刷新、off-air 不刷新、实例 ID、WSPR 频率超过 `UINT32_MAX`；FT8 Call/Grid 复用生产 `DecodedText::deCallAndGrid` 结果，不新增正则；内部 AutoSeq/QSO/TX 文本以 `auto_sequence_state`、`qso_stage`、有 CQ 选中条件的 `cq_state`、`current_tx_text` 输出。
- UDP 合同测试已覆盖空目标和 disabled 客户端仍发出本地 `decode_observed`，以及 primary/secondary 相同目标只收到一个结构化 Status；该测试不是 `MessageClient -> WebState` mirror 端到端验收，未将其宣称为 WebState 镜像去重证明。
- Qt-only 与 MessageClient 语法证据均通过；最终本轮独立构建命令 `cmake --build C:\JTDX64\build-webui-dev-msys2 --parallel 2` 完成 `[1218/1218]`。构建日志：`C:\JTDX64\deps-webui\build-webui.log`；增量测试重建日志：`C:\JTDX64\deps-webui\build-webui-final.log`。
- 最终 CTest 命令：`C:\msys64\usr\bin\bash.exe -lc 'export PATH=/c/JTDX64/159/bin:/mingw64/bin:/usr/bin:/c/Windows/System32:/c/Windows; ctest --test-dir /c/JTDX64/build-webui-dev-msys2 --output-on-failure'`，结果 `100% tests passed out of 14`；日志：`C:\JTDX64\deps-webui\ctest-webui.log`。未运行 `jtdx.exe`，未连接 CAT/PTT/TX，未做浏览器/HIL/部署。
- 阶段结论：P1 代码、独立配置、完整构建和 14 项 CTest 已通过；P2 TCP/API/SSE、浏览器验证、真实 CAT/PTT/TX/HIL/部署仍未开始。Hamlib SDK 和导入库保留在 `C:\JTDX64\deps-webui` 供下批恢复，不提交大二进制到仓库。
- 结束额度（2026-09-08 收尾快照）：账户共享 5 小时剩余 `26%`、周剩余 `70%`；本批不再开启新批，等待用户继续。

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
