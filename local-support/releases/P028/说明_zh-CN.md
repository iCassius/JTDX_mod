# JTDX 2.2.159.028 本机审阅候选（当前）

## 包与版本

- 用途：本机审阅候选；不是公开发行包、验收结论或法律清权意见。
- 源码提交：`bc45bb4d54bbb5729513e5d7763e8f681e320af3`（`修复：避免过早断开 SSE 大快照客户端`）。
- ZIP：`C:\JTDX64\JTDX-2.2.159.028-local-bc45bb4-P028.zip`
- SHA-256：`2771BCB2CB898ECC39A1D1B6796579EF220E14CD90D2B5D9534019FA81715F89`
- ZIP 大小：49,380,789 bytes；142 文件，根项为 `bin/`、`plugins/`、`share/`、`NOTICE`、`NOTICE_zh-CN.md`。暂存、ZIP、独立解压逐项长度及 SHA-256 均 142/142 一致；ValidateOnly 复核通过。
- sidecar：`C:\JTDX64\JTDX-2.2.159.028-local-bc45bb4-P028.zip.sha256`。
- 应用显示版本 `2.2.159.028`；Windows PE 数字版本 `2.2.159.28`（PE 数字资源不保留第四段前导零），产品版本字符串含短哈希 `bc45bb`。
- 固定主窗口构建时间：`2026-09-24 12:50:19 Asia/Shanghai (UTC+08:00)`，由此次 Release 配置时刻生成，写入二进制；不是启动时动态时间。Web 状态从统一 `application_version` 显示版本。
- 手动版本规则：下次候选需将显示后缀改为 `.029`、PE tweak 改为 `29` 并重新构建验证；本项目不自动递增。

## SSE 初始快照失败诊断与修复

全量 CTest 中 `jtdx_web_server_test` 偶发报 `SSE initial snapshot must succeed`。两次原始失败记录保存在 `evidence/P028-version028/ctest-final.log` 和 `ctest-confirmation.log`；单项隔离通过但不能排除全量条件。针对性服务端诊断复现于 `evidence/P028-version028/ctest-sse-server-full.log`：服务端已发 HTTP 头后，298,124-byte SSE snapshot 正在传输时仍有 232,588 bytes 应用队列及 65,652 bytes Qt socket 队列，总量远低于 1MiB 上限；下一份合法 snapshot 到达时，16KiB 瞬时 unread 阈值将这个正在发送的大帧误判为不读客户端并立即 abort。测试端因此只见 200 头、无 event；不是 DLL、固定端口冲突或 SSE 文本帧解析问题。隔离对照日志为 `evidence/P028-version028/ctest-sse-server-isolated.log`。

修复提交 `bc45bb4` 移除了过早的 16KiB 队列深度踢除，保留单事件 1MiB、总发送队列 1MiB 界限以及 2 秒 drain deadline；这样慢读客户端仍会在受界限的期限/容量内释放。完整变更是 `JtdxWebServer.cpp` 中 `send_sse` 的 admission 判断。

## 最终构建与验证

- Release 全量构建退出码 0，日志 `evidence/P028-final-bc45bb4/build-final.log`；最终配置/真实构建时间见 `configure-final.log`、`build-timestamp-final.txt`。
- 针对性 `jtdx_web_server_test`：1/1 通过，含大快照发送及慢读客户端受界限释放，日志 `ctest-targeted-after-fix.log`。
- 相同 PATH、Qt offscreen 环境及原 CTest 顺序下，连续两轮全量 CTest 均 28/28：`ctest-after-fix-1.log`、`ctest-after-fix-2.log`。
- `version_title_test` 退出码 0；PE 资源读回 FileVersion `2.2.159.28`、ProductVersion `2.2.159.28 bc45bb`。Web 夹具 7/7，报告 `browser-fixture-report.json`：页面展示 `JTDX · 浏览器夹具 v2.2.159.028`、375px 横溢 0，双客户端同步/旧 HTTP 回读/重连/epoch 测试均通过。此夹具为本地模拟 HTTP/SSE，不含 MainWindow、CAT 或真实电台。
- CMake install 来自当前 MSYS2 Qt 与本地 Hamlib 4.7.2。与既有 P27 stage 的逐文件比对见 `stage-audit.log`：142 项共有文件中 138 项哈希一致，4 个重建可执行文件不同；65 项 ThirdParty 材料和两份 NOTICE 从 P27 暂存复制并保留哈希。
- ZIP、manifest、清洁解压及只读复核见 `manifest-P028-final-bc45bb4.csv`、`package-verify.log`、`package-validate-only.log`。

## 旧候选、边界与未完成事项

`JTDX-2.2.159.028-local-c545085-P028.zip` 保留在 `C:\JTDX64` 未覆盖，SHA-256 为 `1D3D517EA704F3047F300A8BBB03C95FE41A3393C4468BC3C502F82A2656A13D`；它的单测间歇失败现已由后续候选修复取代，明确标记 `SUPERSEDED`，见 [`c545085-SUPERSEDED_zh-CN.md`](c545085-SUPERSEDED_zh-CN.md)。早期 P28 `2.2.159.2.10` ZIP 也保持不变。

本候选只供本机静态审阅。未启动 `jtdx.exe`，未更改现有安装，未操作 CAT/PTT/TX，未发射；无硬件/HIL、真实 MainWindow→浏览器端到端、LAN/公网或部署验收。Qt/LGPL 与其它第三方组件特定分发义务、间接运行库、`ALLCALL7.TXT`、`CALL3.TXT`、`JPLEPH` 来源及再分发授权仍未完成审计，不得公开分发或声称法律清权完成。未打 Git tag、未 push。
