# 2026-09-28 CAT 故障路由与 TCP 端口验证

## 对使用者的影响

### CAT 故障发生时配置窗口正打开

此前，CAT 运行时故障是否交给主窗口恢复，取决于 Configuration 窗口当时是否可见。配置窗口打开时，故障可能只弹出本地 `Rig failure` 提示，主窗口看不到故障，因而不会进入现有的 AutoSeq/CAT 恢复流程。

现在按 rig 会话的用途决定去向：运行中的 rig 故障始终交给 MainWindow，即使 Configuration 正显示在屏幕上。用户会继续沿用现有的 CAT 故障处理、停止发射判断及限次重连行为；窗口是否打开不再改变这条路径。

### Test CAT 与 Test PTT

- Test CAT 创建的会话标为“配置测试”。该测试会话发生故障时，Configuration 显示本地提示，不把测试失败误报为正在运行的 rig 故障。
- Test PTT 若因当前配置与运行 rig 参数一致而复用运行中的会话，会保留“运行中”身份。此时 CAT 故障仍进入 MainWindow 恢复流程。
- Test PTT 若必须新建会话，则新会话属于“配置测试”，故障只在 Configuration 中提示。
- 配置测试成功后继续编辑时，会话仍保留测试身份，直到用户接受或取消配置。

### 接受或取消 CAT 配置

- 用户接受配置后，当前活动 rig 成为运行会话。若接受前为新参数打开了候选 rig，只有接受成功后才提升为运行会话。
- 用户取消配置时，原来存在运行 rig 的场景会按原逻辑恢复或保留原运行 rig，并标记为运行会话；如果原来没有运行 rig，不会因为取消操作而凭空产生运行会话。
- 所以，打开配置页、试用候选设备和最终接受配置分别有清楚的故障归属；测试会话失败不会触发运行时 AutoSeq 恢复。

### 迟到的 CAT 错误

每次实际创建 rig 都有独立 generation。关闭会话时旧 generation 失效。如果旧设备线程的故障通知排队后才到达，新 rig 已经打开，软件会忽略这条旧通知，避免它关闭新 rig 或错误触发新会话的恢复。

### AutoSeq、DX 与发射行为

本次只修正故障通知进入现有处理流程的条件，不改变 MainWindow 已有恢复策略。是否建立恢复票据仍由现有的 AutoSeq 支持状态、DX 目标和发射意图判断；不满足条件时仍按现有规则停止或忽略自动恢复。

重连后的 PTT 状态门槛也保持原样：只有观察到 rig 在线且 PTT-off，现有流程才释放 DX 并等待新的解码；PTT-on 时继续保留 DX/恢复上下文。此次修改没有降低 PTT/TX 安全门槛，也没有增加自动发射动作。

### TCP 自动端口

Web Server 的生产 TCP/UDP 代码没有改变。仅将自动端口冲突测试改为使用 AnyIPv4 占用首个候选端口，并验证服务跳过冲突端口以及数字 UDP 保留端口。因此用户的 Web UI 地址、端口选择和 UDP 行为均不变；这项变更让回归测试更接近生产监听地址的冲突情形。

## 诊断日志

`jtdx_recovery.log` 的 `rig-control` 记录现在包括：

- `generation`：故障属于哪个 rig 会话代次；
- `session_purpose=runtime` 或 `configuration_test`：会话是运行 rig 还是配置测试；
- `stage=stale_generation_ignored`：旧会话的迟到故障已被丢弃；
- `stage=decision` 与 `forward_to_main_window=true/false`：本次故障交给 MainWindow，还是留在 Configuration 本地提示。

仍只记录故障文本是否存在，不记录 CAT 错误内容、命令、串口端口或凭据。日志用于下次自然故障时还原路由决策，不能追补此前没有 generation/purpose 的旧日志信息。

## 兼容与回退

没有新增用户设置、配置迁移、网络协议字段或持久化状态。改动不影响既有 Web UI/TCP/UDP、CAT 驱动参数或 AutoSeq 配置格式。需要回退时，可在本地通过 `git revert` 撤销本次提交；这不会触碰未纳入提交的本地文件。

## 验证范围

基于 `main` / `11bf7fdd525a54e2d1413617ca2924f4f1446ad6`，使用 Release、MinGW、Qt 5 与 Hamlib 4.7.2 构建。`jtdx`、`jtdx_web_server_test`、`autoseq_cat_diagnostics_contract_test`、`rig_session_policy_test` 均成功构建。8 项定向 CTest 通过 8/8，全量 CTest 通过 32/32；全量运行使用 `QT_QPA_PLATFORM=offscreen`。日志保存在 `local-support/evidence/AutoSeq-CAT-Diagnostics/`。

未启动构建出的 JTDX，未连接或操作真实 CAT/PTT/TX、电台，也未执行真实窗口交互或 HIL。自动化通过不等于真实硬件重连已验证；首次实际运行仍应观察新增 `generation`、`session_purpose` 和 MainWindow 恢复日志之间的对应关系。
