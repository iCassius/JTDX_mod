# JTDX 内置 Web UI：历史文档索引

本索引把当前入口、按日期记录的进度和各阶段专题材料分开。当前源码/本机测试包基线见 [`../../WEB_UI_START_HERE_zh-CN.md`](../../WEB_UI_START_HERE_zh-CN.md)；阶段事实、验证与限制以 [`PROGRESS_zh-CN.md`](PROGRESS_zh-CN.md) 和对应阶段说明为准。下列 P 编号表示历史工作阶段，不表示仍是当前候选。候选包均不自动代表公开发行、硬件验收或法律清权。

## 旧入口完整快照

整理前入口全文保留于 [2026-09-30 历史快照](../../WEB_UI_START_HERE_2026-09-30_历史快照_zh-CN.md)。该快照保留原有阶段交接、验收事实与恢复材料；当前状态请以新版入口和最新进度为准。

## 当前与最近阶段

- **P031 当前源码与本机测试包**：AutoSeq、CAT 会话故障路由及 TCP 自动端口冲突测试；[阶段记录](PROGRESS_zh-CN.md)、[交付说明](../../local-support/releases/P031/说明_zh-CN.md)、[用户版说明](../CHANGELOG_P031_用户版_zh-CN.md)。TCP 生产服务及 UDP 未改。
- **P030 最近 Web UI 行为**：IPv4 默认监听地址和按默认路由显示访问地址；候选、构建与验证证据见 [`local-support/README_zh-CN.md`](../../local-support/README_zh-CN.md) 与 [`local-support/releases/P030/说明_zh-CN.md`](../../local-support/releases/P030/说明_zh-CN.md)。
- **P029**：频谱窗持久显隐；[阶段记录](PROGRESS_zh-CN.md)、[交付说明](../../local-support/releases/P029/说明_zh-CN.md)。

## Web UI 实施与验收阶段

- **P0–P2：需求、架构与只读服务**：[需求与边界](01_需求与边界_zh-CN.md)、[设计/API/安全契约](02_设计_API_安全_zh-CN.md)、[阶段验收矩阵](03_阶段验收矩阵_zh-CN.md)、[进度记录](PROGRESS_zh-CN.md)。
- **P3–P11：原生集成、控制与软件/浏览器验收**：按日期查看 [P3–P11 进度记录](PROGRESS_zh-CN.md)；[P10 软件交付报告](P10-软件交付报告_zh-CN.md)、[P14–P16 RC 历史审计](P14-Release候选审计_zh-CN.md)。
- **P17–P20：Web UI 扩展、QSO 与持久化边界**：[P17](P17-WebUI功能扩展与RC交接_zh-CN.md)、[P18](P18-WebUI-QSO与视口验收_zh-CN.md)、[P19](P19-Web-QSO真实持久化与清理_zh-CN.md)、[P20](P20-真实LogQSO集成边界与候选发布_zh-CN.md)。
- **P21–P24：本机候选、许可证材料与布局纠偏**：专题材料见 [`PROGRESS_zh-CN.md`](PROGRESS_zh-CN.md)；[P23](P23-电台状态紧凑布局与直接操作_zh-CN.md)、[P24](P24-根审阅纠偏_zh-CN.md)。
- **P25–P27：解码/安全门、Web 电台控制及 AutoSeq 修复**：[P25](P25-解码地理字段与电台安全门纠偏_zh-CN.md)、[P26](P26-Web电台控制超时与六按钮收口_zh-CN.md)、[P27](P27-AutoSeq修复本机审阅包_zh-CN.md)。它们是历史候选阶段，不是当前基线。
- **P28、P028、P029**：SSE 快照、版本显示、频谱窗显隐等历史本机候选记录，以 [进度日志](PROGRESS_zh-CN.md) 及 [`local-support/README_zh-CN.md`](../../local-support/README_zh-CN.md) 为索引。`P28` 与 `P028` 为不同历史标识，按文档中的大小写和前导零查找。

## 本机归档与恢复材料

- [本机支持材料和候选归档索引](../../local-support/README_zh-CN.md)：P031/P030/P029 等包、证据、恢复目录及历史材料布局。
- [按日期排序的阶段日志](PROGRESS_zh-CN.md)：含历史检查点、阶段验证和未完成事项；旧记录保留原始时间背景，不代替当前源码复核。
