# P26 Web 电台控制超时与六按钮收口

日期：2026-09-24

状态：实现及软件验证完成，等待根任务复核；不代表验收、发布批准或硬件验证。

## 基线与变更

- 仓库：`C:\JTDX64\jtdx_sourcecode`，分支 `main`；基线 `26cf3655a450c3cf42b30ca57ea556d22bb6f16c`；P26 实现及跟进提交从 `a1eaf35` 至 `b0732b9`，最终实现提交 `b0732b9`。其中补齐启用/停止异步回读及前端 TX 安全门，并收紧窄屏网格宽度约束。
- 修复两个独立故障。第一，`dispatchWebBusiness` 先 prepare/begin 后又把 Radio 送入 `dispatchWebRadio`，重复 prepare 拒绝后使原生处理器从未运行，最终等待超时并置未知锁。第二，服务端 operation 的嵌套 `readback` 未序列化 `tx_enabled` 等安全字段，前端却从该位置判定读回，导致完成结果被误判为未知。
- 电台业务路由现在只 prepare/begin 一次，再经单一 adapter 调用原有 MainWindow 动作并发布新鲜状态；服务端嵌套读回包含 nullable `safety_known`、`tx_enabled`、`transmitting`、`ptt`、`tune`。
- 可见电台区恰为六项：启用发射、终止发射、记录通联、清空窗口、同步、多次解码。同步指解码器同步，不是 CAT 频率同步。启用发射/停止沿用真实原生安全流程；清空、同步、多次解码复用原生控件/槽。
- “记录通联”先打开可编辑 QSO 草稿；取消不落盘，明确确认才进入既有保存路径。测试夹具中的提交不是实际 ADIF/生产持久化证明。
- 普通本地电台动作自身超时不再触发服务级全局未知锁；会改变 TX 或提交 QSO 的高风险动作及非 Radio 业务仍受未确认锁保护。安全 Stop 始终可用，但不会清除旧锁。
- 解码卡横跨工作区全宽；完整消息、实体/省份和动作在窄屏可换行显示；DX 选择按钮跨两行。桌面 Radio 区采用 3×2，窄屏保持两列且页面不横向溢出。

## 验证与证据

- Release 应用及全部测试目标由 `C:\JTDX64\build-webui-p25-release` 构建。该目录为此前已配置的本机 Release 构建，重用其 MSYS2/Qt/Hamlib 工具链；未使用失败的全新 P26 配置目录作为证据。
- 最终实现提交 `b0732b9` 上完整 Release 构建所有应用和测试目标通过，日志：`C:\JTDX64\deps-webui\evidence\P26\final-build.log`。全量 CTest 为 26/26、100%、58.04 秒，日志：`C:\JTDX64\deps-webui\evidence\P26\final-ctest.log`。构建/测试重用此前已配置的 `C:\JTDX64\build-webui-p25-release`；未将失败的全新 P26 配置目录作为验证证据。
- 新增 adapter/控制/服务器/MainWindow 静态契约覆盖单次 dispatch、去重、QSO 草稿、嵌套安全回读及超时锁范围。
- 浏览器使用 `http://127.0.0.1:49154/#fixture` 本机 loopback 内存夹具；对 Enable Tx 的开/关分别检查了 UI 勾选、模拟状态快照和安全门回读。430px CSS 视口实测 `innerWidth=430`、文档宽度 `430`、各卡片宽 `406`、解码行宽 `358` 且无元素越过视口；桌面 1440×1100 也留存截图。截图：`C:\JTDX64\deps-webui\evidence\P26\browser-desktop.png`、`browser-mobile.png`。这是隔离 fixture 及前端的可视/交互证据，不是真实 MainWindow、无线电或 ADIF 集成验收；adapter 行为使用 mock，MainWindow 以静态契约覆盖。

## 本机审阅包

- 最新实现审阅 ZIP：`C:\JTDX64\JTDX-2.2.159.2.10-local-b0732b9-P26.zip`，直接放在 `C:\JTDX64` 根目录；SHA-256 sidecar 与简短说明同处根目录。详细大小、哈希、清洁解压核对结果在本轮打包后补录于此。
- 回退代码时可在仓库执行 `git revert <P26提交>`；本轮不执行回退、不推送、不打标签。

## 边界

没有启动真实 `jtdx.exe`/完整 MainWindow，没有读取用户电台日志，没有连接 CAT 或操作 CAT/PTT/TX，没有发射，没有启动/访问 UDP 服务线程，也没有进行 HIL、LAN/公网或部署验证。浏览器结果只证明隔离软件 fixture 的链路行为。第三方分发权利未在本批复核。旧临时目录按既有约束保留，不宣称已清理。
