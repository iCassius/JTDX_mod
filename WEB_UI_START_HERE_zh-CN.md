# JTDX 内置 Web UI：当前入口与恢复说明

## 当前源码与本机测试包：P031（2026-09-29）

当前源码基线为 P031，应用显示版本 `2.2.159.031`，Windows PE 数字版本 `2.2.159.31`。P031 修复 AutoSeq 回答 CQ 的次数累计和目标释放、普通 CQ 候选短期失败冷却，以及 CAT 运行会话故障路由；TCP 生产服务和 UDP 行为未改。当前本机测试 ZIP 为 `C:\JTDX64\JTDX-2.2.159.031-local-b187262-P031.zip`，SHA-256 `95274274CE774F929FE85D86422ED961B61BD989297D3B57892F6177FEA6B08E`。Release 构建、CTest 32/32 和包完整性记录见 [P031 交付说明](local-support/releases/P031/说明_zh-CN.md) 与 [本机支持材料索引](local-support/README_zh-CN.md)。这是本机测试包，不是公开发行或硬件验收结论。

P031 继承的最新 Web UI 行为来自 P030：新建或未设置绑定地址时默认 IPv4 通配监听；已有 `WebUiBindAddress` 值保留；访问地址按默认路由选择。详见 [P030 记录](docs/web-ui/ARCHIVE_INDEX_zh-CN.md)。P031 对 TCP 只更新了自动端口冲突测试夹具，没有改变 Web 生产监听逻辑。

## 用户变更说明

- [P031 用户版更新说明](docs/CHANGELOG_P031_用户版_zh-CN.md)
- [P031 AutoSeq、CAT 与 TCP 变更记录](docs/CHANGELOG_20260929_P031_AutoSeq_CAT_TCP_zh-CN.md)

## 继续工作前

1. 阅读本入口、[阶段记录与中断恢复日志](docs/web-ui/PROGRESS_zh-CN.md)及 [历史文档索引](docs/web-ui/ARCHIVE_INDEX_zh-CN.md)。
2. 核对仓库分支、HEAD 和工作区；以源码和最新进度记录为准，不把旧阶段描述当作当前行为。
3. 仅按用户当前授权推进。真实 CAT/PTT/TX、HIL、部署和公开发行各自仍需对应授权与验证。

## 文档分工

- 本文件：当前版本、当前入口和继续工作路径。
- [`docs/web-ui/PROGRESS_zh-CN.md`](docs/web-ui/PROGRESS_zh-CN.md)：按日期倒序记录阶段结果、验证和未完成项；较早条目是历史记录。
- [`docs/web-ui/ARCHIVE_INDEX_zh-CN.md`](docs/web-ui/ARCHIVE_INDEX_zh-CN.md)：按阶段查找设计、验收、候选和历史证据。
- [`local-support/README_zh-CN.md`](local-support/README_zh-CN.md)：本机候选、构建恢复与本地归档布局。
