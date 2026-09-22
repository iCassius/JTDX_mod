# P20 真实 LogQSO 集成边界与本地候选交接（2026-09-23）

## 本轮代码与可验证范围

P19 的无当前 DX 请求实际在 `JtdxWebControl::safe_to_dispatch()` 中被拒绝。源代码显示 `MainWindow::m_start2` 初始为 `true`，仅在首次 `handle_transceiver_update()` 后清零；它是启动状态门。此前把它和 `tune/autoTx/iptt` 一起报告为 `tx_path_active` 会把“启动尚未完成”描述成“TX 路径活动”。P20 单独返回 `startup_pending`，原安全拒绝仍生效；真实 AutoTx/Tune/IPTT 路径仍返回 `tx_path_active`。

P20 定向测试调用路径：

- `tests/jtdx_web_control_test.cpp` 直接实例化生产 `JtdxWebControl`，调用 `submit()`；新增断言覆盖 `start2 → startup_pending` 和 `auto_tx → tx_path_active`。既有测试覆盖相同 request ID/payload 返回原结果且不二次 dispatch，也覆盖 QSO confirm 在 draft 关闭且 generation 增长后才通过 `feedback_radio()`。
- `tests/logqso_persistence_test.cpp` 直接构造生产 `LogQSO`，调用 `initWebLogQSO()` 与 `acceptWebQSO()`，在 Qt test data 隔离目录检查真实 ADIF/业务日志追加和不可写失败。修正该测试原有的目录断言：`QDir::isEmpty()` 表示目录内容为空，路径非空应检查 `QDir::path()`。
- `tests/jtdx_web_server_test.cpp` 验证 HTTP 路由到 Control 的请求/回读合同，但它用测试 dispatcher，不调用 MainWindow，也不写 ADIF。

以上测试不是 MainWindow 与 LogQSO 的单次集成验收。MainWindow 的真实路径由 `dispatchWebRadio()` 进入私有草稿函数，再调用其所持有的 `LogQSO`；目前没有可隔离构造该对象的测试入口。构造函数会直接创建 `MessageClient`、`SoundInput/Output`、解码器、配置及音频线程，并绑定生产状态回读/控制对象。当前没有测试构造参数可禁止 UDP/音频副作用并注入合成 DX 上下文。让该构造函数具备注入能力需要跨应用生命周期重构；本轮没有通过放宽安全门、私有内存修改或增加 HTTP/UDP 调试入口绕过这一边界。

## 尚未通过的 Web QSO 集成断言

在 MainWindow 适配与实际 LogQSO 的同一次调用链中，以下项目仍未验证：

- 打开草稿后真实 ADIF/业务日志为零；调用 MainWindow 取消后仍为零。
- 有效合成 DX、时间、频率显式确认后恰好一条 ADIF。
- 相同 `request_id` 重试返回原结果，并且真实 ADIF 记录数不增加。
- 确认期间当前 DX 改变时拒绝；时间/频率上下文变化时拒绝或保持草稿原始上下文，且不误记。
- ADIF 不可写时，Web operation 不完成，草稿/generation 保持未完成，且无成功业务日志；恢复可写目录后的重试结果。

分组件已有的行为不能替代这些集成结论：P19 的 `LogQSO` 测试证明文件追加与写失败原因；`JtdxWebControl` 测试证明 request ID 幂等和回读合同；两者没有共享同一个 Web dispatch 与 LogQSO 调用实例。

## 候选包与验证证据

本地候选由源码功能提交 `46e1b3b` 构建，提交后执行了完整 clean-first 构建，包含 `jtdx`、`qrc_jtdx.cpp` 和全部测试目标。最终回归为 `24/24`、100% 通过。构建和测试日志分别为：

- `C:\JTDX64\deps-webui\evidence\P20\p20-final-clean-build-46e1b3b-20260923.log`
- `C:\JTDX64\deps-webui\evidence\P20\p20-final-ctest-clean-46e1b3b-20260923.log`
- QSO/控制定向测试 `4/4` 通过：`C:\JTDX64\deps-webui\evidence\P20\p20-qso-focused-final-20260923.log`
- 第一次 QSO 定向测试暴露测试目录断言错误，修复后通过；失败原始记录保留：`C:\JTDX64\deps-webui\evidence\P20\p20-qso-focused-20260923.log`

最终本地候选：

- ZIP：`C:\JTDX64\deps-webui\release-current\JTDX-2.2.159.2.10-local-46e1b3b-P20.zip`
- SHA256 sidecar：同路径追加 `.sha256`
- SHA256：`587cf29bf41be0560ac3f0ff05657e27991b532de490a5467e73946483675e64`
- 新鲜解压含 `75` 个文件，直接根目录严格为 `bin/plugins/share`，与构建安装树逐文件比较 `HASH_DIFFERENCES=0`。PE 元数据为 `ProductVersion=2.2.159.2 46e1b3`；候选文件名沿用当前本地构建包命名，不代表正式发行版本或 tag。
- 对包内 `49` 个 EXE/DLL 检查 `276` 个导入项，没有缺失的非 Windows DLL。包内含 JTDX GPL `COPYING` 与 CTY attribution。Qt 与 MinGW runtime 的许可文件位于构建主机 `C:\msys64\mingw64\share\licenses`；本地候选未收集完整第三方许可包，不用于外部分发。
- 依赖与许可记录：`C:\JTDX64\deps-webui\evidence\P20\p20-dependency-audit.log`；P14 原依赖闭包审计仍见 [`P14-Release候选审计_zh-CN.md`](P14-Release候选审计_zh-CN.md)。
- P17/P18 回退 ZIP 位于 `C:\JTDX64\deps-webui\history\rollback`；更早 P14 与 P19 包位于 `C:\JTDX64\deps-webui\history\superseded`。所有 sidecar 均与对应 ZIP 哈希一致。

没有启动 P20 MainWindow。观察到 `UDP 2237` 已由 `C:\Program Files\GridTracker2\GridTracker2.exe` 占用；MainWindow 构造会创建 `MessageClient` UDP socket，再开另一个实例会冲突或建立额外监听。为遵守无新增 UDP listener 的边界，本轮只核对 PE 版本资源，不把它记作 GUI 启动烟测。记录见 `C:\JTDX64\deps-webui\evidence\P20\p20-startup-boundary.log`。候选只表示本地构建和所列软件测试结果；不代表人工 UI、完整 MainWindow QSO、CAT/PTT/TX、HIL、LAN 或部署验收，也不创建正式版本/tag。

## 目录整理记录

- 删除本任务生成的可再生暂存目录：`C:\JTDX64\deps-webui\p14-rc-2.2.159.2.10-aef3434`、`p14-rc-extract-2.2.159.2.10-aef3434-r1`、`p17-install-20260921-ee505fb`、`p17-rc-extract-2.2.159.2.10-ee505fb-r1`、`p18-install-20260921-fa7712f`、`p18-rc-extract-2.2.159.2.10-fa7712f`、`p19-install-20260921-0033f9a`、`p19-rc-extract-2.2.159.2.10-0033f9a`、`temp\P20-46e1b3b`。
- 删除两份 P19 RC 测试启动遗留目录：`C:\Users\cassi\AppData\Local\qttest\JTDX - P19-RC-install - test`、`JTDX - P19-RC-extract - test`。原 P19 Web/LogQSO 测试目录此前已清理。
- 删除临时探针二进制 `abi_probe.exe`、`probe.exe` 和 `qapplication_probe*.exe`，及无引用的 Qt probe 源/输出；保留被旧进度文档引用的 `abi_probe.cpp` 到 `C:\JTDX64\deps-webui\evidence\history`。
- 删除可由最终 clean build/CTest替代的 P20 中间构建/测试日志；保留最终 clean build、最终 CTest、定向通过日志和首次失败日志。
- P19 与 P18 权威日志已归入 `evidence\P19` 与 `evidence\history\P18`。P1-P16 旧日志/脚本仍有历史文档按原绝对路径引用，所以继续保留在 `deps-webui` 根；依赖源、`generated` 资源、活动构建目录、`C:\JTDX64\159` 用户安装和唯一截图也未删除。
- `deps-webui\INDEX_zh-CN.md` 提供当前包、回退、证据和残留历史文件位置索引；P20 staging 目录已不存在。

## 用户操作与回退

候选包内的运行文件仍使用现有 `bin/plugins/share` 根布局。将 ZIP 解压到新目录，核对 SHA256，并保留当前安装和数据目录作为回退点；不得覆盖 `C:\JTDX64\159` 安装。本轮未启动候选 GUI，因此后续运行须先另行安排隔离 UDP/音频/CAT 设置并获得运行授权。若候选出现问题，关闭候选进程，回到现有已验证安装或 P18 回退 ZIP；不要把候选数据目录直接复制覆盖到旧版本。

Web UI 无 token。频率、DX、CQ/AutoSeq 和电台面板控制沿用设置页中显式开启的能力开关；默认状态按设置合同。AutoSeq 启动只会启用现有 decode 驱动引擎，必须等待新的有效解码，不能把启动按钮理解为立即呼叫当前 DX。Web QSO 草稿只有显式确认且实际持久化成功后才完成。

真实 MainWindow Web QSO 成功提交、重试无重复持久化、上下文变化行为、正常窗口关闭、CAT/PTT/TX、HIL 和部署仍需后续人工或独立授权验收。
