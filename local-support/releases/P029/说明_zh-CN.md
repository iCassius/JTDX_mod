# JTDX 2.2.159.029 本机审阅候选

## 包与版本

- 用途：本机独立目录审阅候选；不是公开发行包、验收结论或法律清权意见。
- 源码提交：`ec26de60fb55ed0044a014fa5e6fdb2b0457d4b4`（版本 2.2.159.029），包含 P028 主线修复及 P029 频谱窗行为修复/测试。
- ZIP：`C:\JTDX64\JTDX-2.2.159.029-local-ec26de6-P029.zip`
- ZIP SHA-256：`E624A05A404721FF074B9112C04C2C16376605C47B0972D6F4BD6F7C84743D98`
- ZIP 大小：49,381,555 bytes；142 个文件；清单未压缩总长 137,547,744 bytes。直接根项为 `bin/`、`plugins/`、`share/`、`NOTICE`、`NOTICE_zh-CN.md`。
- sidecar：`C:\JTDX64\JTDX-2.2.159.029-local-ec26de6-P029.zip.sha256`
- 应用显示版本：`2.2.159.029`；Windows PE 数字版本：`2.2.159.29`（资源数字不保留第四段前导零）；ProductVersion 包含 `ec26de`。
- 构建标题固定时间：`2026-09-24 15:12:04 UTC`，由本次 Release 配置生成并编入程序，不是启动时动态时间。
- P028 ZIP 与 sidecar 保持不变；不打 tag、不 push。

## 本候选包含的行为

- P028 版本展示与 SSE 大快照服务修复，以及此前 Web UI 项目内容。未重改右侧 Rx Frequency 的解码过滤/业务规则；其接收门槛、显示筛选和其他写入路径说明见 [`docs/widegraph-visibility-and-rx-window_zh-CN.md`](../../../docs/widegraph-visibility-and-rx-window_zh-CN.md)。
- 频谱窗显隐修复：用户关闭后，JT9/T10/FT4/FT8/JT65/JT9+JT65 模式初始化或切换不再把窗口强制重开；显隐意图写入 `QSettings`，退出后重启可恢复；工具菜单显式打开仍可重新显示。测试夹具覆盖生产窗口策略，但 Qt attribute 检查不等于 Windows 原生窗口管理器/焦点行为的实机验证。
- 用户接受步骤：关闭频谱窗，切换/复位模式，确认仍隐藏；退出并重启后确认仍隐藏；再用工具菜单的 “Wide Waterfall” 显式打开，确认可见。按需另行人工核验启动时焦点与前台行为。

## 构建与验证

- 干净 Release 树：`local-support/build/P029-release/`；配置、完整构建、安装和 CTest 日志均在 `local-support/evidence/P029/`。
- 完整 `all` 构建退出码 0；版本资源读取为 FileVersion `2.2.159.29`、ProductVersion `2.2.159.29 ec26de`。
- 全量 CTest：29/29 通过（含 `widegraph_visibility_test` 和 `version_title_test`），最终耗时 58.14 秒；环境为 Qt offscreen。
- 暂存运行库以已审阅的 P028 安装暂存树补齐；142 项逐项审计中，138 项长度/哈希相同，4 个重新构建的 EXE 不同（`jtdx.exe`、`jtdxjt9.exe`、`udp_daemon_jtdx.exe`、`wsprd_jtdx.exe`）。审计见 `evidence/P029/stage-audit.log`。CMake install 的 `fixup_bundle` 有 `objdump` 缺失警告，因此不单独宣称依赖扫描通过。暂存↔ZIP↔清洁解压逐项长度和 SHA-256 一致 142/142；`-ValidateOnly` 通过。manifest、package verify、只读复核和 sidecar 位于 `local-support/evidence/P029/` 与 ZIP 同目录。

## 边界

本候选不在本任务中启动真实 `jtdx.exe`，不覆盖现有安装，不运行 CAT/PTT/TX，不发射；没有真实 MainWindow/Windows 焦点端到端检查、硬件/HIL、LAN/公网、部署或验收。Qt/LGPL 与其他第三方组件的特定分发义务、间接运行库及数据文件再分发授权仍待审计。仅供本机审阅，不得公开分发，不代表已完成法律清权或发布批准。
