# JTDX 2.2.159.2.9 CAT 恢复修复发布说明

任务编号：`JTDX-CAT-RECOVERY-20260906`。Windows PE 四段资源版本保持 `2.2.159.2`，显示版本后缀更新为 `.9`。

## 修复内容

- FTX-1 CAT 硬故障停发后，确认实际 PTT 关闭即释放旧 DX；恢复票据随后等待新的完整解码批次，才尝试恢复原 QSO。
- 明确发给本台的原台标准报文优先于增量新候选；按既有 QsoHistory 状态恢复 Tx2/Tx3/Tx4/Tx5，周期、模式、报告和标准消息在 Enable Tx 前完成。
- 保留复合呼号完整形式，基本呼号只用于消息匹配。
- 识别 Tx5 开始时已写入 `FIN` 的部分 73 历史，避免旧 `FIN` 或普通旧报告造成伪完成/重复 73。
- 用户停发、Escape、UDP HaltTx、禁用、清除、目标/模式/波段/Tx 周期变化取消恢复；内部 CAT 故障停发保留票据。
- Hamlib ERR 回调和 FTX-1 轮询日志补充受限诊断；Hamlib 4.7.2 DLL、CAT 安全门和底层 EPROTO 行为保持不变。

## CAT 现场证据边界

本任务保留并明确记录 2026-09-06 现场证据：04:43 左右本地 `ON4IQ` 两次发射后，`get_ptt` 返回 `EPROTO`，按既有安全路径硬停发并重连；同夜空闲轮询还出现 `get_vfo`/`get_ptt` 交替失败，单次操作计数为 1，但聚合失败达到 3 次而升级。FTX-1 `Polling=0` 的实际轮询周期为 500 ms。Hamlib CAT 解析/响应不匹配、串口链路或设备状态均可能造成 `EPROTO`；旧版日志没有保存失败字节，因此不能证明底层传输根因，也不能把本次上层恢复修复描述成底层 EPROTO 已修复。新增有界 ERR 日志只为后续复现保留事实证据。

## 验证

本 `.9` 包明确使用 `WSJT_ENABLE_OMNIRIG=OFF`；源码默认选项仍为 `ON`。权威 `build-159.2.9` MinGW64 Release 增量重编译完成，完整 CTest `13/13` 通过，含真实 `QsoHistory` 事件链、生产恢复门控源码断言、FTX-1 CAT 策略和 OmniRig OFF 构建门。运行时依赖闭包、ZIP 和 SHA256 由同一结果提交后的交付流程复核。

未执行真实电台、CAT/PTT、音频或发射验证。运行时包若交付，仅允许包含顶层 `bin/`、`plugins/`、`share/`；SHA256 sidecar 放在 ZIP 外部；不覆盖已安装的 `C:\JTDX64\159`。

设计和源码基线见 [DESIGN_2.2.159.2.9_zh-CN.md](DESIGN_2.2.159.2.9_zh-CN.md)、基线 `b8d94539fea8a163b137dbd0fe30681e2dca2252` 与结果提交 `e5a4381e6ac496c7d1a0088e39c1e3a08d94ad2a`。
