# P27 AutoSeq 修复本机审阅包

## 候选与源码

- 阶段：P27，本机审阅候选，不是公开发行包或验收结论。
- 产品版本：JTDX 2.2.159.2。
- 源码提交：`3c086af9aa6ff6f9ac57513277740cda5145683e`（`修复 AutoSeq CQ 冷却来源与筛选覆盖`）。
- 本包包含 P26 Web UI 基线及其后的 AutoSeq 修复。旧 P26 包 `JTDX-2.2.159.2.10-local-b0732b9-P26.zip` 的源码提交是 `b0732b9`，早于 AutoSeq 修复提交，不包含本修复。
- 本机旧安装 `C:\JTDX64\159\bin\jtdx.exe` 的产品版本资源显示提交标识 `651bf9`；不能将该程序等同于本次 P27 构建。

## 软件验证和包校验

- Release 完整构建：`local-support/evidence/P27/final-build.log`；安装及 CMake bundle 依赖核验：`local-support/evidence/P27/install.log`。
- 全量 CTest：27/27 通过，100%，58.47 秒；日志：`local-support/evidence/P27/final-ctest.log`。
- ZIP：`C:\JTDX64\JTDX-2.2.159.2.10-local-3c086af-P27.zip`，49,376,951 bytes。
- SHA-256：`9B0A3AC0D537982320F1F1566439ABC7116DAE9F4B41E8A06D56A181C7B02DD8`；sidecar 位于 ZIP 同目录。
- 142 个文件；直接根项为 `bin/`、`plugins/`、`share/`、`NOTICE`、`NOTICE_zh-CN.md`。独立解压目录为 `C:\JTDX64\jtdx_sourcecode\local-support\extract\P27-clean-extract-3c086af`；142/142 文件长度与 SHA-256 和安装暂存树一致，差异 0。manifest 与校验记录见 `local-support/evidence/P27/manifest-P27-3c086af.csv`、`package-verify-P27.log`。
- 安装暂存树：`C:\JTDX64\jtdx_sourcecode\local-support\staging\P27-runtime-stage-3c086af`。P25/P26 ZIP、依赖和受限临时项未删除或覆盖；P25 清洁解压副本只做了目录归档移动，见根目录整理记录。

## 对“解码区出现大量 X”的检查

用户提到的现象没有在可读取证据中复现：最近 `202609_ALL.TXT` 最后记录为 2026-09-23 15:29 UTC 左右，未检出连续 6 个以上的 X；检查时没有正在运行的 JTDX 窗口。当前本机旧安装也不是 P27 构建。源码显示解码文本从解码器输出直接进入显示对象，未发现将正常消息统一替换成 X 的处理；这不足以排除特定声卡/信号、解码边界或旧版本的运行问题。

因此本批没有针对该现象改源码。若再次出现，请保留带 UTC/SNR/频率/模式的原始解码行及对应接收 WAV 或截图，才能判断是否属于射频/音频引起的误解码、数据传输/格式错位或可复现的软件缺陷。

## 验证边界

本次没有启动候选 JTDX，没有连接或操作 CAT/PTT/TX，没有发射真实无线电控制/信号，没有做 HIL、LAN/公网验证或部署。构建和自动化测试通过不等于完整 MainWindow、硬件或发布验收；第三方源码对应关系、Qt LGPL 替换/重新链接权利和数据再分发许可仍须独立核查。
