# JTDX 2.2.159.028 本机审阅候选

## 包与版本

- 用途：本机审阅候选；不是公开发行包、验收结论或法律清权意见。
- 源码提交：`c545085da5bf0ed3e249cbcf76aa4f1c5f186394`（`测试：等待 Web 版本状态完成渲染`）；版本实现提交 `a648c12166c29f53c3e5b529236389ae8d8a4f40`。
- ZIP：`C:\JTDX64\JTDX-2.2.159.028-local-c545085-P028.zip`
- SHA-256：`1D3D517EA704F3047F300A8BBB03C95FE41A3393C4468BC3C502F82A2656A13D`
- ZIP 大小：49,380,841 bytes。142 个文件；直接根项为 `bin/`、`plugins/`、`share/`、`NOTICE`、`NOTICE_zh-CN.md`。打包脚本核对暂存与 ZIP、再核对独立解压与暂存，均为 142/142 文件长度和 SHA-256 一致；`-ValidateOnly` 复核通过。
- SHA-256 sidecar：`C:\JTDX64\JTDX-2.2.159.028-local-c545085-P028.zip.sha256`。
- 应用内显示版本为 `2.2.159.028`。Windows PE 固定数字版本为 `2.2.159.28`（Windows 版本资源不保留第四段的前导零）；产品版本字符串包含源码短哈希 `c54508`。
- 主窗口沿用原标题信息并在末尾附加固定构建时间：`2026-09-24 12:14:12 Asia/Shanghai (UTC+08:00)`；此值是在本次 Release 配置时生成并写入二进制，不是每次启动时钟。Web 状态中的版本直接取统一 `application_version`。
- 版本递增为手动操作：下个候选应将显示后缀改为 `.029`、PE 数字 tweak 改为 `29`，并重新配置、构建、测试和打包；当前不会自行递增。

## 构建与验证

- 复用源码内 Release build `local-support/build/dependency-audit-configure`。首次完整 Release 构建退出码 0，日志 `evidence/P028-version028/build.log`；夹具时序测试修正后增量重建退出码 0，日志 `verification-build.log`。
- 初始实现提交 `a648c12` 的全量 CTest 为 28/28（`ctest.log`）。最终源码提交 `c545085` 的两次全量复跑在 `jtdx_web_server_test` 报 `SSE initial snapshot must succeed`，分别记录于 `ctest-final.log` 和 `ctest-confirmation.log`；同一测试独立直接复跑退出码 0（`jtdx-web-server-focused.log`）。这是最终提交测试中的间歇现象，仍需后续复核，不宣称最终源码稳定全绿。版本/标题测试 `version_title_test` 独立退出码 0（`version-title-test.log`）；PE 资源读回为 FileVersion `2.2.159.28`、ProductVersion `2.2.159.28 c54508`。
- 隔离浏览器夹具 7/7 通过，报告 `evidence/P028-version028/browser-fixture-report.json`：Web 版本显示 `JTDX · 浏览器夹具 v2.2.159.028`；375px 手机窄屏横向溢出 0；双客户端 SSE 收敛、旧 HTTP 回读防回滚、Last-Event-ID 重连和 epoch 切换用例通过。夹具是本地模拟 HTTP/SSE，不含 MainWindow、CAT 或真实电台。
- CMake 安装记录 `install.log`。依赖运行库来自当前 MSYS2 Qt 与本地 Hamlib 4.7.2；P27→P028 暂存逐文件对照记录 `stage-audit.log`：142 项共有文件中 138 项哈希一致，4 个本次重建的 JTDX 可执行文件不同。65 项 `share/doc/ThirdParty` 材料和两份根级 NOTICE 从既有 P27 暂存复制，沿用其哈希。未将 `test-runtime` 内容打入包。
- 候选 ZIP、manifest 与独立解压校验见 `manifest-P028-version028.csv`、`package-verify.log`；只读复核输出 `VALIDATED=142 files` 及同一 SHA-256。完整 CTest 的成功轮、两次失败复跑均保留，不能只引用成功轮而隐藏复测结果。

## 使用边界与未完成事项

只供本机静态审阅。请解压到新的独立目录；本次未启动候选 `jtdx.exe`、未改动现有 JTDX 安装、未操作 CAT/PTT/TX、未发射，也未进行 HIL、真实 MainWindow→浏览器端到端、LAN/公网或部署验证。旧 P28 ZIP、sidecar 与说明保持不变。

Qt/LGPL 与其它第三方组件在具体分发方式下的许可/对应源码义务、间接运行库，以及 `ALLCALL7.TXT`、`CALL3.TXT`、`JPLEPH` 的来源与再分发授权尚未完成审计。因此不得公开分发或宣称法律清权完成。未打 Git tag、未 push。
