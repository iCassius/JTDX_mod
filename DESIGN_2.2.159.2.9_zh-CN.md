# JTDX 2.2.159.2.9 CAT 恢复设计说明

任务编号：`JTDX-CAT-RECOVERY-20260906`  
源码基线：`b8d94539fea8a163b137dbd0fe30681e2dca2252`  
结果提交：`e5a4381e6ac496c7d1a0088e39c1e3a08d94ad2a`；不推送、不打标签、不安装。

## 目标与边界

Sep 6 的 FTX-1 现场记录为本地 `ON4IQ` 两次发射后，PTT 读取出现 Hamlib `EPROTO`，随后 CAT 重连但 AutoSeq 没有继续原 QSO。修复只协调 JTDX 上层恢复状态：不修改 Hamlib 源码或 DLL，不放宽 FTX-1 的 PTT-first 轮询、PTT 不确定保护或 CAT 硬故障停发语义。

现场同夜还出现空闲 `get_vfo`/`get_ptt` 交替失败：单次操作失败计数为 1，但聚合失败达到 3 次而升级；FTX-1 `Polling=0` 对应 500 ms 轮询。`get_ptt` 的 `EPROTO` 可能来自 CAT 解析/响应不匹配、串口链路或设备状态，旧版日志没有保存失败字节，不能确认底层传输根因。本任务只增加受限 ERR/聚合诊断和上层 AutoSeq 恢复，不能声称已经修复 Hamlib 的底层 EPROTO。

Hamlib 4.7.2 分析锚点：

- [ftx1_tx.c](https://github.com/Hamlib/Hamlib/blob/4.7.2/rigs/yaesu/ftx1/ftx1_tx.c)
- [ftx1_vfo.c](https://github.com/Hamlib/Hamlib/blob/4.7.2/rigs/yaesu/ftx1/ftx1_vfo.c)
- [newcat.c](https://github.com/Hamlib/Hamlib/blob/4.7.2/rigs/yaesu/newcat.c)

## 恢复事件链

```text
CAT 故障且 AutoSeq 有真实发射意图
  -> 保存完整中断 DX 与规范化匹配键
  -> 内部停发，保留恢复票据
  -> CAT 在线并读回实际 PTT-off
  -> 立即清除旧 DX，等待新的自动解码批次
  -> 只观察明确发给本台的标准报文
  -> QsoHistory.autoseq 选择既有阶段
  -> 恢复完整 DX、接收周期、报告和标准消息
  -> 通过既有 TX/PTT 守卫后再 Enable Tx
```

恢复票据在 CAT 在线但 PTT 仍为 On 时不前进。解码批次必须在确认 PTT-off 后开始；批次边界消息不算新消息。手动解码、磁盘解码和自由文本不消费票据。

## 状态与 QsoHistory 规则

`AutoSeqRecoveryPolicy` 只保存票据、批次时间、完整呼号、基本呼号匹配键和首个 73 是否在故障时发射中，不直接操作 UI、CAT 或发射。

- `RCALL`、`RREPORT`、`RRREPORT`、`RRR`、`RRR73`、`R73` 继续既有 AutoSeq 的 Tx2、Tx3、Tx4、Tx5 阶段。
- 复合呼号（例如 `OZ7KJ/P`）完整保留到 DX 输入和消息生成；比较来报时单独使用 `OZ7KJ` 基本呼号。
- Tx5 在发射开始时就可能把历史状态写成 `FIN`。旧 `FIN` 不能证明对方已经收到 73；只有本批原台实际发来 `RRR`、`RR73` 或 `73` 才可处理完成。
- 故障时 Tx5 在途且本批收到完成报文，重发 Tx5；若 Tx5 已不在途且本批收到完成报文，只完成状态，不再次发射；普通旧 `RREPORT`、旧 `FIN` 或无关台消息均不触发。
- 没有原台续联也没有真实可自动起呼的新候选时，DX 保持为空，票据继续等待。新候选只有在现有自动目标/定向应答分支确实可 arm 时才消费票据；历史状态或不可 arm 的暂选不会重新占用 DX。

## 取消条件与诊断

用户停发、Escape、UDP `HaltTx`、禁用 Enable Tx、清除 DX、编辑目标、改变模式/波段/Tx 周期均取消票据。CAT 内部故障触发的 `haltTx` 由内部标记与用户停发区分，保留票据。Hamlib 错误回调和 FTX-1 聚合失败计数写入由 `JtdxLocalLog` 统一受 256 KiB/单 `.1` 轮换限制的 `jtdx_recovery.log`；Hamlib 的策略互斥和写入器互斥分别只承担各自职责，不能据此宣称 MainWindow 全局线程安全。

## 验证边界

新增 `autoseq_recovery_policy_test` 使用真实 `QsoHistory::message` 事件链验证 RCALL/RREPORT/RRREPORT/RRR/RRR73/R73、部分 73、完整呼号和无关消息；`ui_contract_test` 检查解码观察、PTT-off 门、恢复顺序、取消边界、聚合诊断和 fallback 保留票据。FTX-1 CAT/PTT、音频、功率/SWR、长时间挂机和实际发射仍需 HIL；本任务不启动 JTDX、不连接电台、不执行 CAT/PTT/TX。

权威 `build-159.2.9` 使用 MinGW64 Release、`WSJT_ENABLE_OMNIRIG=OFF` 完成增量重编译，完整 CTest 为 `13/13` 通过；默认 OmniRig 选项仍为 `ON`。这组结果只证明源码和离线策略测试通过，不替代真实电台 HIL。
