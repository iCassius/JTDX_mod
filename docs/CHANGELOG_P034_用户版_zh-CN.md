# JTDX 2.2.159.034 本机审阅候选更新说明

本文件对应本地 P034 review candidate，版本号为应用显示 `2.2.159.034`、Windows PE 数字版本 `2.2.159.34`。源码仍以 `main@50334d31e3f65936e1936c052078e0151780a1c8` 为基线，P033/P034 改动保留在 dirty 工作树；该提交不包含候选的未提交内容。本候选不是公开发行。

## 变更

- retired 的 Transceiver worker 不再让同一旧实例中的排队 `start`/online `set` 重新进入 driver；恢复由新 rig session 实例完成。新 PTT-on admission 与最终 driver 调用前的撤销门阻止尚未进入 driver 的旧 queued PTT-on。已经进入 driver 的调用无法撤回。
- TX1–TX8 的实际待发编码文本共享 selector 和规范化身份。等待发送期间，选中 payload 改变会取消旧 token；未选文本变化不取消，Tune/WSPR 继续沿各自既有路径。
- legacy 在线状态回调可以刷新 PTT 状态观察时间；strict GET 后端仍要求当前代次、已知状态和递增的成功读取 serial。观察时间刷新本身不算 AutoSeq TX 进度；实际 progress 仍要求匹配请求的 PTT off→on 状态边沿（边沿来源按后端区分）或既有音频启动确认。
- 相同 CAT TX 目标避免重复配置写入；旧 rig session 的 resolution/TCI 回调按 generation 过滤。FTX-1 CAT 增加只读 PTT GET 健康观察与手动刷新结果说明。
- PTT 诊断补充请求 context/diff、owner/hold、FTX-1 readiness 与 recovery 信息；安全导出增加有限 CAT 标量/固定枚举。端点、主机、凭据、TX 文本与不可安全解码的 Variant 内容不会作为这些新字段导出。

## 使用边界

CAT health observer 观察当前活跃会话的 API/readback 结果，不自动重连、不关闭串口，也不控制 TX。5 秒只用于标记尚未完成的 health poll 为观察性 suspect，不是 PTT readiness 超时。Hamlib `RIG_OK` 和协议同步守卫通过只表示 API 层 GET 返回并通过协议检查，不证明串口线上往返或电台响应。点击 `readFreq` 时保留旧 `sync_transceiver(true)` 调用，然后才请求独立 GET；旧调用继续使用 `enforce_mode_and_split=false` 默认值，可能保留 split-off/TX 频率清零的兼容副作用。

AutoSeq 初始 owner 只有在 120 秒无实际进展，并且完整新鲜 decode batch、目标 context 匹配、在线、目标选择后已知 PTT-off、无 pending/recovery/operator hold 等安全条件同时满足时才允许清理。unknown/offline/PTT-on/ticket 会 fail-closed 保留目标；手动 DX 与已进入 QSO 的目标继续受保护。本候选不承诺将任何停滞目标自动清空。

历史 `004430` 与 `052800` 记录缺少足够的 request/generation/readback/context 关联字段，无法确认是否由同一根因造成。旧 queued worker 风险对应的代码路径早于 P033；静态/软件夹具证明的是代码路径行为，不是现场症状根因证明。

## 验证与限制

本说明仅记录候选改动与验证边界；自动化检查和包完整性检查不等于 GUI、现场电台或射频验收。

没有启动 JTDX，没有安装到用户使用中的正式安装目录，没有连接真实 CAT/串口/PTT/TX，没有进行 HIL。未验证 long-idle 时的实际 COM 释放根因；OmniRig 在该 Release 配置中关闭，仅进行静态路径审阅。自动化检查不等于 GUI、现场电台或射频验收。
