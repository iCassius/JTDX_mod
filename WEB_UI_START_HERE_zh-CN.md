# JTDX 内置 Web UI：阶段入口与恢复说明

本目录记录 JTDX 内置轻量 Web UI 的需求、边界、设计和阶段证据。它是开发过程文档，不代表 Web UI 已经实现，也不代表已经启动 JTDX、连接电台或完成浏览器/HIL 验证。

## 当前状态

- 阶段：`P0 文档与事实基线`，本轮已完成；后续阶段未开始。
- 基线：分支 `main`，HEAD `582296c8d140e3f23bad785de1f7d9585b212197`，工作树在本轮开始时干净。
- 代码根目录：`C:\JTDX64\jtdx_sourcecode`。用户需求中的 `jtdx\_sourcecode` 按当前实际仓库路径解释。
- 本轮范围：只整理文档和恢复状态；没有修改业务代码、CMake、配置、端口，没有启动 JTDX，没有连接 CAT/PTT/真实电台，没有部署。
- 已确认：主程序已有 Qt5 Network、`MessageClient`、`MessageServer` 和 `JTDX_BUILD_LOCAL_TESTS`；当前没有 Web UI 实现。
- 进程边界：Web 功能必须零新增进程、零新增常驻线程，优先使用 JTDX 主 Qt 事件循环。当前程序已有 `proc_jtdxjt9` 解码子进程，Web 任务不得把它误写成 Web 新增进程，也不得为了 Web 重构或删除它。

## 继续工作的最短路径

1. 先读本文件、`docs/web-ui/PROGRESS_zh-CN.md`，再读与当前阶段对应的设计文档。
2. 检查 `git status --short --branch`、`git log -1 --oneline`；若基线或工作树与记录不符，先更新恢复日志，不覆盖已有修改。
3. 每一阶段只做一个可审查批次：先源码事实和文件计划，再实现，再做该阶段静态/单元/API 验证，最后写恢复记录并提交中文 commit。
4. 每批开始读取当前配额；发现额度接近上限时，立即把已完成、未完成、命令和证据写入恢复日志，再停止，不把“请求已发出”当成完成。
5. 未经另外授权，不启动 JTDX、不连接真实电台、不执行 CAT/PTT/TX/HIL、不向群晖部署。

## 阶段顺序

| 阶段 | 边界 | 交付重点 | 当前状态 |
| --- | --- | --- | --- |
| P0 | 文档与事实基线 | 需求、网络/进程安全边界、API 草案、验收矩阵、恢复入口 | 已完成 |
| P1 | 状态模型 | `JtdxWebState`、状态新鲜度、解码上限、事件循环安全读接口；只读数据接入 | 未开始 |
| P2 | 只读服务器 | `JtdxWebServer`、TCP 端口生命周期、`/`、`/healthz`、`/api/v1/state`、`/api/v1/decodes`、SSE | 未开始 |
| P3 | 设置/菜单/前端骨架 | Web UI 设置 Tab、端口/绑定策略、菜单入口、内置资源、响应式只读页面 | 未开始 |
| P4 | 普通控制 | `JtdxWebControl`、频率切换、过期解码 ID、DX 选择、状态回读 | 未开始 |
| P5 | 高风险控制 | CQ/AutoSeq 启动、停止流程、二次确认、幂等/超时/冲突和状态回读 | 未开始 |
| P6 | 整体验证 | 自动化合同、浏览器手测、异常隔离、回归和交付报告 | 未开始 |
| HIL | 独立授权 | 真实 CAT/PTT/发射、设备反馈、长时间运行和无线电行为 | 未授权/未开始 |

P1 至 P6 每次只推进一个阶段；用户检查额度后再继续。HIL 不属于普通阶段的默认验收。

## 事实锚点与构建入口

以下是 P0 读取源码所得事实，后续实现前仍需以当时源码复查：

- `main.cpp` 创建 `MainWindow`；`mainwindow.cpp` 持有两个 `MessageClient`，并连接现有状态、解码和控制信号。
- `MessageServer.cpp` 是既有 UDP 服务；Web 服务器只能新增独立 TCP 监听，禁止复用、改写或镜像为新的 UDP 监听器。
- `Configuration.hpp/.cpp` 已有 UDP 地址/端口和频率、收发器等配置入口；Web 配置应新增独立键名，不能改动现有 UDP 键。
- `mainwindow.cpp` 的 `band_changed()`、`setRig()`、`handle_transceiver_update()`、`status_update()` 和解码路径是后续状态/频率接入的事实核对点。当前不能把请求写入的目标频率当作 CAT 实际回读频率。
- 现有 CQ、AutoSeq、Enable Tx、停止流程有 UI 槽函数和内部副作用；后续必须先抽取或复用业务入口，不能从 Web 模拟鼠标点击或直接调用 PTT。
- `CMakeLists.txt` 生成 `jtdx`、`jtdxjt9` 等目标；Qt5 Network 已存在；`JTDX_BUILD_LOCAL_TESTS` 控制本地 CTest 目标。本阶段没有配置或构建新的目标。

已探测到的工具版本仅用于恢复参考：`C:\msys64\mingw64\bin\cmake.exe` 4.4.0、`g++.exe`/`gfortran.exe` 16.1.0 Rev5、`qmake-qt5.exe` Qt 5.15.19；未验证本项目配置/编译兼容性。后续先确认独立构建目录和依赖，不自动升级工具链。

建议的本地构建/测试形式（只在实现阶段、确认依赖和独立构建目录后使用）：

```powershell
cmake -S C:\JTDX64\jtdx_sourcecode -B C:\JTDX64\build-webui-dev -DJTDX_BUILD_LOCAL_TESTS=ON
cmake --build C:\JTDX64\build-webui-dev --config Release
ctest --test-dir C:\JTDX64\build-webui-dev -C Release --output-on-failure
```

上述是构建模板，不是本轮执行结果；现有历史构建目录和 `CMakeCache.txt` 不作为当前证据。

## 文档索引

- [需求、边界与改进项](docs/web-ui/01_需求与边界_zh-CN.md)
- [设计、API 与安全契约](docs/web-ui/02_设计_API_安全_zh-CN.md)
- [阶段验收矩阵](docs/web-ui/03_阶段验收矩阵_zh-CN.md)
- [进度与中断恢复日志](docs/web-ui/PROGRESS_zh-CN.md)
