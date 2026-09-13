# JTDX 内置 Web UI：阶段入口与恢复说明

本目录记录 JTDX 内置轻量 Web UI 的需求、边界、设计和阶段证据。当前 P3 生产 service/QAction、P4 生命周期/dispatch 基础和 P4 频率 HTTP 隔离入口已完成；完整 MainWindow 窗口人工验收留到 P6，仍不代表已经启动 JTDX、连接 JTDX 电台或完成 HIL/部署验证。频率 HTTP 生产写入口因有限 operations/SSE 完成回读尚未接入而强制关闭，不能把隔离夹具的 `accepted/pending` 当作 CAT 完成。当前 Web 本批结果基线为 `2ca51ec`；较早 CAT `.10` 交付提交 `a89c9da` 仅作外部历史参考。

## 当前状态

- 阶段：`P4 频率 HTTP 基础切片（生产 fail-closed）`，P0/P1/P2/P3 已完成，P4 Service/Server/Control 生命周期与 frequency dispatch 基础已完成；完整 MainWindow 窗口人工验收留到 P6。有限 operations 摘要/SSE 完成与超时回读尚未接入，生产频率 POST 保持关闭。
- 基线：本批从分支 `main`、HEAD `7c79be7` 开始，结果提交为 `2ca51ec`；P4 HTTP 契约见 [`docs/web-ui/P4-FREQUENCY-HTTP_契约_zh-CN.md`](docs/web-ui/P4-FREQUENCY-HTTP_契约_zh-CN.md)，当前结果与恢复点见进度日志。
- 代码根目录：`C:\JTDX64\jtdx_sourcecode`。用户需求中的 `jtdx\_sourcecode` 按当前实际仓库路径解释。
- P3 历史范围：增加 Web UI 配置 Tab、持久化摘要令牌、MainWindow 唯一服务生命周期、菜单入口和 Qt Resource 原生深色响应式只读页；服务仍为单进程主 Qt 事件循环，不新增 UDP/线程/进程或控制 API。
- 已确认：主程序已有 Qt5 Network、`MessageClient`、`MessageServer` 和 `JTDX_BUILD_LOCAL_TESTS`；P1 状态模型、P2 只读服务、P3 代码/资源/测试/文档、P4 生命周期/dispatch 与 HTTP 隔离基础均已有本地提交，operations/SSE 业务回读和用户可用生产控制仍未实现。
- CAT 交付基线：代码提交 `328cc7a` 的错序检测及后续 artifact code HEAD `9984c38` 已由 `a89c9da` 记录最终 Release、完整 CTest `19/19`（含既有 `ftx1_cat_policy_test`）；该测试并非本 Web 文档批次新增。Web 仍只呈现实际状态，不把 CAT 构建证据或 HTTP `accepted` 当成设备回读或通联完成。
- 进程边界：Web 功能必须零新增进程、零新增常驻线程，优先使用 JTDX 主 Qt 事件循环。当前程序已有 `proc_jtdxjt9` 解码子进程，Web 任务不得把它误写成 Web 新增进程，也不得为了 Web 重构或删除它。

## 继续工作的最短路径

1. 先读本文件、`docs/web-ui/PROGRESS_zh-CN.md`，再读与当前阶段对应的设计文档。
2. 检查 `git status --short --branch`、`git log -1 --oneline`；若基线或工作树与记录不符，先更新恢复日志，不覆盖已有修改。
3. 每一阶段只做一个可审查批次：先源码事实和文件计划，再实现，再做该阶段静态/单元/API 验证，最后写恢复记录并提交中文 commit。
4. 每批开始读取当前配额；当五小时窗口剩余低于 `20%` 时，立即把已完成、未完成、命令和证据写入恢复日志，只做有界收尾、静态检查、精确提交和交接，不开启新的实现面；不把“请求已发出”当成完成。此前日志中的 `30%` 收尾阈值仅适用于历史批次，当前批次以 `20%` 为准。
5. P0 文档阶段不启动 JTDX、不连接真实电台、不执行 CAT/PTT/TX/HIL、不向群晖部署。用户后续继续到 P3/P6 时，可在隔离配置、`Rig=None` 且无真实 CAT 连接的条件下进行本地浏览器验证；开始前必须证明不会自动连接硬件。真实 CAT/PTT/TX 和无线电行为仍需单独授权。

## 阶段顺序

| 阶段 | 边界 | 交付重点 | 当前状态 |
| --- | --- | --- | --- |
| P0 | 文档与事实基线 | 需求、网络/进程安全边界、API 草案、验收矩阵、恢复入口 | 已完成 |
| P1 | 状态模型 | `JtdxWebState`、状态新鲜度、解码上限、事件循环安全读接口；只读数据接入 | 已通过（独立构建/14 项 CTest；未启动 JTDX/HIL） |
| P2 | 只读服务器 | `JtdxWebServer`、TCP 端口生命周期、`/`、`/healthz`、`/api/v1/state`、`/api/v1/decodes`、SSE | 已通过（loopback/API/CTest；未启动 JTDX/HIL） |
| P3 | 设置/菜单/前端骨架 | Web UI 设置 Tab、端口/绑定策略、菜单入口、内置资源、响应式只读页面 | 代码/资源/CTest/浏览器夹具复验已完成；完整 MainWindow 窗口人工验收留到 P6 |
| P4 | 普通控制 | `JtdxWebControl`、频率切换、过期解码 ID、DX 选择、状态回读 | 生命周期/dispatch 与 frequency HTTP 隔离基础已完成；生产 gate=false，operations/SSE 回读、frequency CAT 业务完成和 DX 入口留后续批次 |
| P5 | 高风险控制 | CQ/AutoSeq 启动、停止流程、二次确认、幂等/超时/冲突和状态回读 | 未开始 |
| P6 | 整体验证 | 自动化合同、浏览器手测、异常隔离、回归和交付报告 | 未开始 |
| HIL | 独立授权 | 真实 CAT/PTT/发射、设备反馈、长时间运行和无线电行为 | 未授权/未开始 |

P1 至 P6 每次只推进一个阶段；用户检查额度后再继续。HIL 不属于普通阶段的默认验收。

下一步是补齐有限 operations 摘要及 SSE 完成/超时回读，并经独立验收后重新评估生产 gate；当前已有 HTTP 隔离入口，但没有用户可用的生产频率控制、频率 CAT 生产回读或 HIL 证据。

本批最终证据：构建日志为 `C:\JTDX64\deps-webui\p4-frequency-http-final-build-2ca51ec.log`，其中确认重新编译 `jtdx_web_service_test` 的 `JtdxWebService.cpp`/`JtdxWebServer.cpp`；全量 CTest 日志为 `C:\JTDX64\deps-webui\p4-frequency-http-final-ctest-2ca51ec.log`，`19/19` 通过。此前 `LastTestsFailed` 或旧 service 二进制状态不作为本批结论。

## 事实锚点与构建入口

以下是 P0 读取源码所得事实，后续实现前仍需以当时源码复查：

- `main.cpp` 创建 `MainWindow`；`mainwindow.cpp` 持有两个 `MessageClient`，并连接现有状态、解码和控制信号。
- `MessageServer.cpp` 是既有 UDP 服务；Web 服务器只能新增独立 TCP 监听，禁止复用、改写或镜像为新的 UDP 监听器。
- `Configuration.hpp/.cpp` 已有 UDP 地址/端口和频率、收发器等配置入口；Web 配置应新增独立键名，不能改动现有 UDP 键。
- `mainwindow.cpp` 的 `band_changed()`、`setRig()`、`handle_transceiver_update()`、`statusUpdate()` 和解码路径，以及 `MessageClient::status_update()`，是后续状态/频率接入的事实核对点。当前不能把请求写入的目标频率当作 CAT 实际回读频率。
- 现有 CQ、AutoSeq、Enable Tx、停止流程有 UI 槽函数和内部副作用；后续必须先抽取或复用业务入口，不能从 Web 模拟鼠标点击或直接调用 PTT。
- `CMakeLists.txt` 生成 `jtdx`、`jtdxjt9` 等目标；Qt5 Network 已存在；`JTDX_BUILD_LOCAL_TESTS` 控制本地 CTest 目标。本阶段没有配置或构建新的目标。

已探测到的工具版本仅用于恢复参考：`C:\msys64\mingw64\bin\cmake.exe` 4.4.0、`g++.exe`/`gfortran.exe` 16.1.0 Rev5、`qmake-qt5.exe` Qt 5.15.19；未验证本项目配置/编译兼容性。后续先确认独立构建目录和依赖，不自动升级工具链。

建议的本地构建/测试形式（仅模板；只在实现阶段、确认依赖和独立构建目录后使用，不代表本轮执行结果）：

```powershell
C:\msys64\mingw64\bin\cmake.exe -G "MinGW Makefiles" -S C:\JTDX64\jtdx_sourcecode -B C:\JTDX64\build-webui-dev -DCMAKE_BUILD_TYPE=Release -DJTDX_BUILD_LOCAL_TESTS=ON -DWSJT_ENABLE_OMNIRIG=OFF -DCMAKE_C_COMPILER=C:\msys64\mingw64\bin\gcc.exe -DCMAKE_CXX_COMPILER=C:\msys64\mingw64\bin\g++.exe -DCMAKE_Fortran_COMPILER=C:\msys64\mingw64\bin\gfortran.exe
C:\msys64\mingw64\bin\cmake.exe --build C:\JTDX64\build-webui-dev --parallel 2
C:\msys64\mingw64\bin\ctest.exe --test-dir C:\JTDX64\build-webui-dev --output-on-failure
```

上述模板沿用当前 Hamlib-only 验证边界（`WSJT_ENABLE_OMNIRIG=OFF`）；现有历史构建目录和 `CMakeCache.txt` 不作为当前证据。若工具路径、Qt 或生成器不同，先记录探针结果再调整，不自动升级依赖。

## 文档索引

- [需求、边界与改进项](docs/web-ui/01_需求与边界_zh-CN.md)
- [设计、API 与安全契约](docs/web-ui/02_设计_API_安全_zh-CN.md)
- [阶段验收矩阵](docs/web-ui/03_阶段验收矩阵_zh-CN.md)
- [进度与中断恢复日志](docs/web-ui/PROGRESS_zh-CN.md)
