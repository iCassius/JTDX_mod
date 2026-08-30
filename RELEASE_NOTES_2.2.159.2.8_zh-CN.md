# JTDX 2.2.159.2.8 自动起呼清理修复发布说明

显示标题和关于窗口版本为 `JTDX v2.2.159.2.8 自动起呼版 By BI7KGD`。Windows PE 四段版本仍为 `2.2.159.2`，程序 ProductVersion 在其后附加最终源码 revision；`.8` 是显示版本后缀，不改变 PE 资源字段布局。

## AutoSeq 修复

- `RFIN`、`RCQ`、`SCALL` 和跳过 TX1 时的 `SREPORT` 统一使用回答 CQ 终止策略；Hound 保持独立路径。
- 回答 CQ 次数阈值与转呼他台终止是独立 OR 条件。阈值仍受 `SeqAnswerCQCount` 和次数限制控制；转呼终止仅在 `m_reply_other` 且频谱与当前 TX 保护范围重叠，或 `SeqHaltTxReplyOther` 开启时成立。
- 自动目标 priority `5/6`、`7/8`、`13/14`、`15/16`、`20/21`、`22/23` 达到阈值或符合条件的转呼后，停止并关闭 Enable Tx、清除 DX、重置计数并回到 `CALLING` 待机；本轮不重新选择，也不写 `calllist`。
- 普通非自动目标继续使用原有 `calllist`、`m_counter`、single-shot 和 legacy cleanup 语义。若 `readFromStdout` 已经提交停发，AutoSeq 收尾不重复 haltTx。
- 保留 `canForceCandidate` 对活动 TX、pending first 73、已处理周期和仅 Tx5 后抢占的保护；定向呼叫、手工双击、Hound/WSPR 等无关行为不变。

## FTX-1 与构建边界

本交付包明确使用 `WSJT_ENABLE_OMNIRIG=OFF`，是仅用于 Hamlib 后端验证的 Hamlib-only 测试包：

- FTX-1 使用 Hamlib model `1051`；Hamlib 源码和运行时 DLL 未修改。
- OFF 配置不包含 `OmniRigTransceiver`、`dumpcpp` 或 ActiveQt wrapper，不注册 OmniRig Rig 1/2；源码默认 `WSJT_ENABLE_OMNIRIG=ON` 的普通 Windows 构建语义不变。
- 包只包含可直接覆盖现有 `C:\JTDX64\159` 的 `bin/`、`plugins/`、`share/` 三个运行时目录，不包含启动脚本、验收脚本、README、发布说明、SHA256 清单或构建临时文件。

## 覆盖方式

用户应先关闭正在运行的 JTDX、GridTracker 及相关 CAT/PTT 软件，自行备份 `C:\JTDX64\159`，再将同名包根目录下的 `bin/`、`plugins/`、`share/` 覆盖到现有 `C:\JTDX64\159`。本任务不替用户执行覆盖，也不删除既有目录。

## 自动验证与未完成 HIL

使用 MinGW64 完成独立 clean Release 构建，`WSJT_ENABLE_OMNIRIG=OFF`，并以单并行运行完整 CTest，共 `12/12` 通过；同时检查了 OFF cache、最终 `jtdx` 链接元数据、依赖闭包、PE 版本、显示版本字符串、ZIP 单根目录和外部 SHA256。

自动测试不等于真实电台 HIL。本次未启动 JTDX，未连接或操作 CAT/PTT、音频、电台，也未发射；真实 FTX-1 CAT/PTT、功率/SWR、自动起呼转呼清理和新候选接管仍需用户自行人工验证。
