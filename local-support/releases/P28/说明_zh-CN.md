# JTDX P28 低延迟状态同步本机审阅候选

## 包与源码

- 用途：本机审阅候选；不是公开发行包、验收结论或法律清权意见。
- 源码 HEAD：`c196a091053eabd1a330a5942c8f2c8a953dedc6`（`Web HTTP 回读防旧状态覆盖`），父提交 `302b062`（低延迟状态推送）；祖先包含 P27 源码提交 `3c086af9aa6ff6f9ac57513277740cda5145683e`。
- ZIP：`C:\JTDX64\JTDX-2.2.159.2.10-local-c196a09-P28.zip`
- ZIP SHA-256：`8F95E9F79C993650A703D63C225830E6669891186FAD05BC5ED99D9671337D37`
- ZIP 大小：49,380,321 bytes。共 142 个文件；ZIP 顶层直接包含 `bin/`、`plugins/`、`share/`、`NOTICE`、`NOTICE_zh-CN.md`。安装暂存和独立解压树各 142 文件、137,544,101 bytes；脚本逐项核对相对路径、长度、SHA-256，差异 0。
- SHA-256 sidecar：`C:\JTDX64\JTDX-2.2.159.2.10-local-c196a09-P28.zip.sha256`。

## 构建与验证

复用已配置的源码内 Release build `local-support/build/dependency-audit-configure`；没有在源码根或 `deps-webui` 创建构建/安装/解压中间目录。该构建树 CMakeCache 指向本源码树、MinGW Makefiles、Release 和 `C:\msys64\mingw64` Qt。最终应用与测试目标全量构建通过；CTest 27/27、56.94 秒通过。源码内容在提交 `c196a09` 前已完成上述构建与测试，提交只固化同一份已验证工作树内容。

- 构建日志：`local-support/evidence/dependency-audit/live-state-sync-root-review-final-build.log`
- CTest 日志：`local-support/evidence/dependency-audit/live-state-sync-root-review-final-ctest.log`
- CMake 安装及 bundle 检查：`local-support/evidence/P28/install.log`；7 个可执行文件依赖集经 CMake bundle verifier 检查有效。
- P27→P28 全树逐文件对照：`local-support/evidence/P28/P27-P28-tree-comparison.csv`，142 个路径中 138 个大小/哈希一致；恰有本次构建的 4 个 JTDX 可执行文件哈希不同。
- 逐文件清单：`local-support/evidence/P28/manifest-P28.csv`
- 打包校验摘要：`local-support/evidence/P28/package-verify.log`
- 只读复核：`local-support/evidence/P28/package-validate-only.log`，输出 `VALIDATED=142 files` 及相同 ZIP SHA-256。
- 独立解压目录：`local-support/extract/P28/`；暂存树：`local-support/staging/P28/`。

运行时来源按 CMake 安装日志与 SHA-256 核对：包内 7 个 `Qt5*.dll` 与当前 `C:\msys64\mingw64\bin` 对应文件逐项同哈希，清单见 `local-support/evidence/P28/qt-runtime-manifest.csv`。包内 Hamlib 4.7.2 `libhamlib-4.dll` 为 11,981,824 bytes、SHA-256 `72F09E0BC1118C11AE0652612D58BD04BA620C4DCF2D4181D8DC39D4AF60D28E`，与 P27 候选和测试启动所用 DLL 同哈希；本次 CMake 安装日志实际记录的来源为 `local-support/dependencies/hamlib-4.7.2/bin/`。并未将 GUI 测试的 `test-runtime` 目录作为 Qt runtime，也未把 P25 整套 Qt 运行时混入。P28 与 P27 暂存树的 77 个共有安装文件中，所有运行依赖逐项同哈希；4 个本次重建的 JTDX 可执行文件不同。三项 `rigctl` helper executable 按现有 CMake 安装规则从本地 P25 clean-extract 复用，P27 包亦含同一文件；安装日志和两阶段 manifest 可追溯该边界。

安装后缺少的 65 个 `share/doc/ThirdParty` 文件，从 P27 暂存树补齐，并逐文件核对数量、长度和 SHA-256；另将两份根级 NOTICE 从 P27 暂存树复制并校验。该复用基于 P27 与 P28 的依赖文件一致性核验，不代表第三方再分发义务已完成。

## Web 状态传播与证据边界

桌面控件变化由 MainWindow 的 20ms 单次计时器合并成当前完整投影；`JtdxWebServer` 再以 20ms 单次计时器合并 SSE 推送。Server 另有 1 秒 revision/operation 周期检查、2 秒快照周期兜底和 10 秒 SSE heartbeat。以上是源代码定时器设置，不是端到端耗时保证。

提交版浏览器夹具最终 6/6 通过：桌面视口 985×780，手机窄屏 375×780 且横向溢出 0；两个客户端顺序切换均收敛，首次到两端 DOM 约 16.3ms、两次切换约 32.2ms；模拟 SSE 写入到浏览器 DOM 约 1.8ms。覆盖旧 HTTP 回读不能回滚新 SSE、epoch 切换、断线重连及 Last-Event-ID/resync。此夹具使用模拟 HTTP/SSE 服务加载实际仓库 Web UI，不含 MainWindow、CAT 或真实电台。首次提交后浏览器复跑曾有一次等待超时，随后立即复跑 6/6 通过；两次运行分别记录在 `local-support/evidence/P28/browser-fixture-run-history.log`，最终成功轮报告在 `local-support/evidence/dependency-audit/live-state-browser-fixture-report.json`。

## 使用与回退

将 ZIP 解压到新的、独立目录供静态审阅；不要覆盖现有安装目录，不要与当前 JTDX 并行启动。若停止审阅，停止使用该独立目录并继续保留 P27 候选即可；本次没有安装到用户配置目录，也没有改动 P27。当前不建议实际启动 P28；若后续获批测试，应先规划独立配置目录与电台隔离，再单独安排启动和硬件验证。

本次未启动候选 `jtdx.exe`，未操作 CAT/PTT/TX，未发射，未做真实 MainWindow→浏览器端到端测量、HIL、LAN/公网或部署测试。未打 Git tag，未 push。

## 尚未完成的发布审计

- Qt/LGPL 与其它第三方组件的许可文本、可替换/重新链接方式及按具体分发方式所需完整对应源码尚未完成法律审查。
- 间接运行库的来源/许可证和对应源码未全部收齐。
- `ALLCALL7.TXT`、`CALL3.TXT`、`JPLEPH` 的精确来源与再分发授权仍待核实。
- 因此本 ZIP 仅为本机审阅候选，不能公开分发或宣称已完成第三方清权。
