# JTDX 内置 Web UI：进度与中断恢复日志

本文件是阶段交接记录。每次继续工作先更新“本次批次”表，再开始代码或测试；每次暂停前必须补齐命令和证据。不要把未来计划写成已完成事实。

## 当前恢复点

- 当前阶段：`P0 文档与事实基线`。
- 本批状态：已完成并提交；下一步等待用户检查额度后进入 P1。
- 基线分支/提交：`main` / `582296c8d140e3f23bad785de1f7d9585b212197`。
- P0 结果提交：以本文件所在提交为准，提交后必须用 `git show --stat --oneline HEAD` 和 `git status --short --branch` 复核。
- 工作树预期：干净；任何后续未提交修改必须在恢复记录中列出。
- 额度策略：每批开始读取当前额度；额度接近上限先写恢复记录再暂停。本任务不创建 5 小时定时任务或自动化监控。

## P0 本批记录

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

### 下一步

P1 先确认 Qt/MinGW/CMake 的独立构建探针，不自动升级依赖；再只做状态模型和数据接入事实/最小实现：确定 `MessageClient`/MainWindow 状态信号的可复用边界、解码字段和新鲜阈值，增加 `JtdxWebState` 及单元测试。不开放控制，不监听 TCP，不新增设置或菜单。

## 恢复命令

```powershell
Set-Location C:\JTDX64\jtdx_sourcecode
git status --short --branch
git log -3 --oneline --decorate
git show --stat --oneline HEAD
rg -n "MessageClient|MessageServer|status_update|decode|handle_transceiver_update|band_changed|process_Auto|AutoSeq|haltTx|on_stopTxButton_clicked|on_pbCallCQ_clicked" main.cpp mainwindow.cpp mainwindow.h MessageClient.cpp MessageClient.hpp MessageServer.cpp MessageServer.hpp Configuration.cpp Configuration.hpp
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
