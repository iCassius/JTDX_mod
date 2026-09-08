# JTDX 内置 Web UI：进度与中断恢复日志

## P3 批次记录（2026-09-09）

- 基线：`main`、`ed04be6`；工作树在本批开始时干净。执行模型为 Luna high；本批开局额度记录为 5 小时、周额度 86%。
- 本批目标：完成 P3 配置 Tab、MainWindow 唯一服务生命周期、Qt Resource 原生只读页面、菜单入口及对应夹具/回归测试；不进入 P4/P5 控制。
- 已冻结文件清单：`Configuration.hpp/.cpp/.ui`、`mainwindow.h/.cpp/.ui`、`JtdxWebServer.hpp/.cpp`、`CMakeLists.txt`、`tests/jtdx_web_server_test.cpp`、`tests/ui_contract_test.cpp`、`resources/web-ui/index.html`、`style.css`、`app.js`、`docs/web-ui/WEB_UI_P3_验收_zh-CN.md`、本日志及入口文档。
- 已完成：确认 P2 `JtdxWebServer`/`JtdxWebState` 接口；冻结 Web 专用持久键、自动端口排除 UDP 数字、仅本机/明确 LAN 地址、SHA-256 令牌摘要和零新线程/进程/UDP 边界；开始把服务鉴权改为摘要恒定时间比较。
- 已完成：配置 Tab 与 MainWindow 接入、正式资源路由和前端 SSE、P3 服务/资源回归、独立构建/CTest、浏览器夹具复验、日志留档和事实文档；动态 Configuration QWidget 保存/取消/重载/重复打开及菜单重复点击留待后续 P3 验收。
- 安全边界：令牌原文只在设置对话框的临时编辑控件和用户浏览器内存中出现；不得进入 QSettings、URL、HTTP HTML、日志或服务成员。应用/HIL/真实 CAT/PTT/TX 尚未启动，浏览器验证需先使用隔离夹具。

### P3 实施进展 checkpoint（2026-09-09，历史检查点；最终结果见下节）

- 已完成：配置 Tab/专用持久键、摘要令牌、MainWindow 唯一生命周期、菜单/打开/重启按钮、Qt Resource 页面、fetch-SSE 令牌输入/断线/epoch ID/未知值处理、浏览器夹具 `--serve-browser`。
- 已验证：`C:\JTDX64\build-webui-dev-msys2` 全量构建成功，权威日志为 `C:\JTDX64\deps-webui\build-webui-p3-final.log`；`ctest --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure` 实际结果 `100% tests passed out of 15`，权威日志为 `C:\JTDX64\deps-webui\ctest-webui-p3-final.log`。
- 已收尾：浏览器夹具带演示解码和陈旧状态复验、`git diff --check`、中文事实文档和精确本地提交均已完成。未做动态 Configuration 对话框保存/取消/重载/重复打开、菜单重复点击、真实 JTDX 隔离启动、CAT/PTT/TX/HIL、部署。

本文件是阶段交接记录。每次继续工作先更新“本次批次”表，再开始代码或测试；每次暂停前必须补齐命令和证据。不要把未来计划写成已完成事实。

### P3 最终收尾检查点（2026-09-09）

- 最终二进制已重建：`cmake --build C:\JTDX64\build-webui-dev-msys2 --parallel 2`，退出码 `0`；日志 `C:\JTDX64\deps-webui\build-webui-p3-final.log`。
- 最终二进制全量回归：`ctest --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure`，`100% tests passed out of 15`，退出码 `0`；日志 `C:\JTDX64\deps-webui\ctest-webui-p3-final.log`。
- 浏览器夹具已用最终二进制启动：`http://127.0.0.1:49152/#fixture`；令牌仍只由进程标准输出一次提供。夹具包含 6 条带 DF、网格、fresh/is_new 和 `<script>` 文本的演示解码，7 秒后推进单调时钟越过 5 秒新鲜度阈值，并设置有界 120 秒退出。父任务复验确认桌面/390px 移动布局、最新优先解码、陈旧中文提示、空实际频率、AutoSeq/TX 文本和重复连接单 TCP 流；断线后的旧快照/陈旧标识由只读代码检查和错误令牌断线观察支持。未在 120 秒自动退出瞬间观察页面；父任务已确认夹具进程不存在。
- 前端最终修订：显示业务状态更新时间、解码年龄、实际频率 `rig_fresh` 状态、AutoSeq、DF、每条解码新鲜度/新旧标识；使用 `textContent`；fragment 演示标识不进入 HTTP；连接循环有 runId、局部 AbortController、每次读取看门狗和旧快照陈旧标识。
- 尚未验证：真实隔离 `jtdx.exe`、Configuration QWidget 动态保存/取消/重载/重复打开及菜单重复点击、CAT/PTT/TX/HIL、部署和长期浏览器耐久性。P3 代码、资源、测试和文档完成后即停，不进入 P4。

## 当前恢复点

- 当前阶段：`P3 设置/菜单/只读前端`（代码、资源、构建、CTest 和夹具浏览器复验已完成；动态 QWidget 设置/菜单交互待后续 P3 验收）。
- P2 历史状态：已完成；P1 已完成（源码结果提交 `0420a21`，文档结果提交 `b788d0e`，收尾提交 `e27c9bb`）。P3 工作树起始基线为 `ed04be6`；本批结果以单一本地中文提交记录。
- 基线分支/提交：`main` / `ed04be6`（P3 批次开始实际读数）。
- P0 初始结果提交：`02153b0`（提交后修订不 amend，使用 `git log --oneline -- docs/web-ui` 追踪后续文档提交）；提交后必须用 `git show --stat --oneline HEAD` 和 `git status --short --branch` 复核。
- 工作树预期：本批结束前列出并精确提交本批修改；不得覆盖其他工作。
- 模型策略：按用户要求使用 `gpt-5.6-luna`、`high`；不自行升级模型或推理档位。
- 配额策略：本批开始账户共享快照为 5 小时剩余 93%、周剩余 80%；本批收尾快照（2026-09-08）为 5 小时剩余 26%、周剩余 70%。这些是账户共享读数，不是本任务独占额度；当前仍高于新批阈值 20%，但本批不再扩展范围，只收尾并写恢复记录。本任务不创建定时任务或自动化监控。

## P2 本批记录（进行中 checkpoint）

### 2026-09-08 P2 批次开始检查点

- 开始额度：账户共享 5 小时/周窗口均剩余 `100%`；本批额度低于 `20%` 时不再开启新批，只收尾并记录恢复点。
- 基线：`main` / `e27c9bb`，工作树开始时干净；代码根目录 `C:\JTDX64\jtdx_sourcecode`。
- 本批目标：在主 Qt5 事件循环内增加惰性、默认不监听的 `JtdxWebServer`，仅提供只读首页、`GET /healthz`、`GET /api/v1/state`、`GET /api/v1/decodes` 和 SSE `GET /api/v1/events`；严格限制 GET/HTTP/1.1、Host/Origin、Bearer 鉴权、连接/头/事件/队列、端口生命周期和 epoch/revision 重同步。
- 明确边界：不自动启动、不启动 JTDX、不连接 CAT/PTT/TX/HIL、不新增线程/进程/UDP、不改现有 UDP/CAT/解码逻辑；P3 设置/菜单/正式前端与 P4 控制均不在本批。
- 文件计划（先冻结后改动）：`JtdxWebServer.hpp`、`JtdxWebServer.cpp`、`CMakeLists.txt` 最小源文件接入；`tests/jtdx_web_server_test.cpp` 真实 loopback TCP 夹具；本文件及 `03_阶段验收矩阵_zh-CN.md`/`WEB_UI_START_HERE_zh-CN.md`/必要设计文档按事实同步。必要时只在 `mainwindow.*` 增加惰性持有/测试注入接口，默认不监听。
- SSE 尺寸检查：在测试中实测 500 条完整 decode JSON；采用实测依据选择单事件与单客户端待发送上限，确保合法满 500 快照首次 HTTP/SSE 成功，并验证慢客户端在有界背压下断开；无 revision 变化时通过周期性 snapshot/新鲜度投影保持可见状态更新。`server_epoch`/`web_server_state` 由服务投影，不篡改业务新鲜度。
- 验收计划：真实 loopback TCP 覆盖自动/手动/占用/UDP 数字排除/停止重启/重复启停、JSON/API、鉴权/Host/Origin、malformed/slow、SSE reconnect+epoch/背压/多客户端；再跑原 14 项 CTest。未运行 `jtdx.exe`、真实 CAT/PTT/TX/HIL/部署。

### 2026-09-08 P2 批次结果（已完成）

- 已实现：`JtdxWebServer.hpp/.cpp` 使用 Qt5 `QTcpServer`/`QTcpSocket`，只运行在创建线程的 Qt 事件循环；默认惰性、不自动监听；加入 `wsjt_qt`，但未在 `MainWindow` 自动创建或启动。`QPointer` 状态生命周期保护，State 销毁停服。
- HTTP 合同：仅 `GET`/HTTP/1.1；严格 CRLF、Host/Origin、无 query/body/TE/重复头；头上限 16KiB、头时限 5 秒、读取缓冲有界；首页仅无敏感引导；`/healthz`、state、decodes、SSE 均要求 bearer；所有控制 POST 路径不执行并返回 404/405；响应 drain 有界，异常只关闭当前连接。
- 端口和安全：默认绑定 `127.0.0.1`；自动端口仅 49152..49251，排除传入 UDP 数字且不执行 UDP bind；手动端口 1024..65535；LAN 仅允许显式具体地址、强令牌和匹配的 HTTP Origin；令牌不进入 URL/响应/日志。
- SSE：snapshot 的 `server_epoch`/`web_server_state` 是服务层投影，业务 revision/freshness 不被伪造；每 2 秒周期 snapshot 使无 revision 变化的新鲜度仍可观察，另有 heartbeat；`Last-Event-ID` 超限拒绝，历史不保留时发送 `resync_required` 和完整 snapshot；事件上限 1MiB、单客户端待发送上限 1MiB，慢连接服务端回收。500 条长文本 decode 的实测完整 snapshot 约 `278 KiB`（`/api/v1/decodes` 双字段 HTTP body 约 `554 KiB`），因此事件上限未误设为 512KiB。
- 测试文件：`tests/jtdx_web_server_test.cpp` 真实 loopback TCP 覆盖自动/手动/占用/UDP 数字排除、停止/重启/epoch、重复启动、首页/health/state/decodes、满 500 JSON 读回、令牌/Host/Origin/query/body/POST、CRLF/部分头超时、SSE 完整初次/重同步、连接上限及恢复、慢 SSE 背压回收、State 销毁停服和 timer/socket 生命周期。
- 最终构建命令：`C:\msys64\mingw64\bin\cmake.exe --build C:\JTDX64\build-webui-dev-msys2 --parallel 2`，主目标 `jtdx.exe` 与 `jtdx_web_server_test.exe` 均链接完成；退出码 `0`，日志：`C:\JTDX64\deps-webui\build-webui-p2-final.log`。
- 最终测试命令：`C:\msys64\mingw64\bin\ctest.exe --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure`；最终 `100% tests passed out of 15`，退出码 `0`，日志：`C:\JTDX64\deps-webui\ctest-webui-p2-final.log`。此前一次全量运行因慢连接测试等待窗口不足报告 `14/15`；最终夹具观察窗口已延长，慢 SSE 服务端回收和其余 15 项均通过。
- 未运行：`jtdx.exe`、真实 CAT/PTT/TX/HIL、浏览器、部署；P3 设置/菜单/正式前端与 P4/P5 控制仍未开始。
- 收尾额度：账户共享窗口约 5 小时剩余 `29%`、周剩余 `89%`；本批不再开启新阶段，等待用户检查 P2 提交。

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
