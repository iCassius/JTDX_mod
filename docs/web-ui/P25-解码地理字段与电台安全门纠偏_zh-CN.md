# P25 解码地理字段与电台安全门纠偏

日期：2026-09-23

状态：实现及软件验证完成，等待根任务复核；不宣称验收、发布批准或硬件验证。

## 基线与实现

- 仓库：`C:\JTDX64\jtdx_sourcecode`；分支 `main`；基线 `2d5ee7b0d75f439b4016a42b0ea69cbd0434c9f8`。
- 代码提交：`05ee60d5de8ae1d29b00df6d23badcf12cd435c6`（`修复 Web 解码投影与无线电停止安全门`）。实现限于 15 个代码/测试文件，含解码投影、Web UI、控制门与测试；未改 UDP 协议或主 CAT/PTT/TX 执行机制。
- 解码投影保持完整消息（24 字符）、SNR/DT/DF/模式和既有 `MessageClient` 数据流；JSON 地理字段区分 DXCC entity、continent 与中国省份，前端不再把洲代码当国家显示。移除独立可编辑 DX 卡，将读回状态与解码选择关联，选择仍以 `decode_id` 匹配真实解码项。
- 动作级门控允许选择解码目标及不发射的本地动作，不把所有动作都误套为电台频率/CQ 的强门。频率与开始发射相关动作继续严格门控；安全 Stop 可越过未知反馈锁并压过在途动作，但不会清除未知锁。
- 解码列采用内部水平/垂直滚动。页面本身在桌面、平板宽度和窄手机视口不横向溢出。

## 根因及验证

旧解码字符串按 23 字符截断，样式网格同时把消息和操作按钮挤出视口；地理显示取了逗号分隔字段第一项（大洲缩写），没有取实体名称；独立编辑 DX 卡和安全门过度聚合分别导致选择状态与解码项脱节、无发射动作被错误拒绝以及 Stop 被未知反馈锁阻挡。

- 完整 Release 构建目录：`C:\JTDX64\build-webui-p25-release`。权威最终构建日志：`C:\JTDX64\deps-webui\evidence\P25\final-build-retry.log`（第一次并行构建的自动生成 UI 头文件竞争见 `final-build.log`；重跑后所有目标成功）。
- 全量 CTest：25/25 通过，100%；日志：`C:\JTDX64\deps-webui\evidence\P25\final-ctest.log`。包含投影集成、动作门控矩阵、控制服务和完整应用目标。
- 浏览器为本机 loopback 的 HTTP/SSE 夹具，使用测试定义的解码样例及生产投影/消息状态/服务器路径；它不是真实 MainWindow 或实时 DXCC 数据验收。视口实测为 1280、550、390 CSS 像素：各自 `document.scrollWidth == viewportWidth`；解码容器可横向滚动并在滚动末端完整显示“选择 DX”按钮，12 条解码、实体与河北样例可见。测量：`C:\JTDX64\deps-webui\evidence\P25\browser-measurements.json`。
- 初始/滚动中段/滚动末端截图：`C:\JTDX64\deps-webui\evidence\P25\responsive-{1280,550,390}-{initial,scroll-mid,scroll-end}.png`。它们记录测试夹具，不证明真实电台状态。

## 本地候选包

- ZIP：`C:\JTDX64\deps-webui\release-current\JTDX-2.2.159.2.10-local-05ee60d-P25.zip`。
- ZIP SHA-256：`9D7A80E9EBDE4DC7A1C77E63A53E7DF0C5C30AA25F3E4E4ECB68CBE7E743C6FC`；sidecar：`C:\JTDX64\deps-webui\release-current\JTDX-2.2.159.2.10-local-05ee60d-P25.zip.sha256`。
- 141 文件清单：`C:\JTDX64\deps-webui\release-current\manifest-P25-05ee60d.csv`。ZIP 独立解压于 `C:\JTDX64\deps-webui\temp\P25-clean-extract-05ee60d`；根项为 `bin/`、`plugins/`、`share/` 和 `NOTICE_zh-CN.md`，141/141 文件均与 staging manifest SHA-256 一致。包仅供本机审阅，不是公开发行物。

## 未验证边界

未启动完整 JTDX MainWindow 做人工 GUI 验收；未连接 CAT 或进行实际频率/DX 设备回读；未操作 CAT/PTT/TX、未发射、未访问 UDP 控制流、未做 HIL；未做 LAN/公网访问或部署。第三方分发义务及数据文件许可尚未完成法律审查。测试结果不等价于真实设备、生产或发布验收。现状提交至根任务等待复核，不标记为已接受。

## 文件与日志

- 安装日志：`C:\JTDX64\deps-webui\evidence\P25\install.log`；构建配置及工具路径：`configure-install-tools.log`。`final-build.log` 保留了 UI 自动生成头文件竞态的第一次失败，完整重建成功的最终日志是 `final-build-retry.log`。
- 归档 SHA-256 sidecar 与 141 行文件 manifest 位于 `release-current`，独立提取目录用于核对；旧 P23/P24 构建、暂存目录与归档未改动。
- P23/P24 的历史文档和归档保持原样；本记录只定义 P25 候选状态。
