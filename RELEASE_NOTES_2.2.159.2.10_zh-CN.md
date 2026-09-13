# JTDX 2.2.159.2.10 CAT 同步测试包发布说明

显示标题为：`JTDX v2.2.159.2.10 自动起呼版 By BI7KGD`。Windows PE 四段版本保持 `2.2.159.2`，`.10` 是产品显示和测试包标识。

## JTDX-CAT-SYNC-20260913

基线为 `cde1cb064b37e942f287d32b962fe6c26956ea8d`，CAT 同步修复代码提交为 `328cc7a1df9fcc488d15b512067f5264425f8dd2`。修复仅识别同步 Hamlib debug callback 中明确的 `newcat_get_cmd wrong reply`，用 thread-local generation 快照消费当前 FTX-1 `do_poll()` 的新事件；即使最终返回 `RIG_OK`，也记录 `operation=protocol_sync` 并进入已有的有界故障恢复策略。空闲确认 PTT 关闭时连续三轮才升级，PTT 未知、意图/实际开启或转换保持期间立即硬失败；普通 warning、可选 meter 错误、其他机型、PTT/CAT 安全门和 Hamlib DLL 保持原语义。

现场依据为 `202609_ALL.TXT` 第 96572 行的 UTC `05:26:30` `CQ R9OOF NO14` 后 DX 残留，以及 `FT→FA→MD→SH→SM→TX→VS→FT` 错序链。第二现场 `202609_ALL(1).TXT` SHA256 为 `CD4E566CC482A9474385D273F3911C30C0D742210A1DE3D8797EA884BF94D78B`，日志头为旧版 `JTDX v2.2.159.2.844fd76`；其中 70 次 TX 属于旧版特殊目标重试漏控复现，不能作为当前源码修复失效证据。原始证据没有完整串口坏帧，因而本修复不宣称已证明或消除 Hamlib 底层 EPROTO 根因。

## 构建与交付

- 配置：MinGW64 Release，`WSJT_ENABLE_OMNIRIG=OFF`；源码默认值仍为 `ON`，Windows 四段资源布局不变。
- 权威构建目录：`C:\JTDX64\build-webui-dev-msys2`。
- 目标 `ftx1_cat_policy_test` 通过；完整 CTest 结果、ZIP 路径、大小和 SHA256 在最终交付验证提交中补录。
- 测试包目录与 ZIP 仅提供可直接覆盖运行目录的 `bin/`、`plugins/`、`share/` 三个顶层目录；不包含外层脚本或文档。SHA256 sidecar 放在 ZIP 外部。
- 未启动 JTDX，未连接 CAT/PTT，未进行真实发射、音频或 HIL；未安装、未覆盖 `C:\JTDX64\159`。
