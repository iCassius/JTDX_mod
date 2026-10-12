# JTDX 2.2.159.035 本机审阅候选更新说明

本文件对应 P035 本机 review candidate：应用显示版本 `2.2.159.035`，Windows PE 数字版本 `2.2.159.35`。基线仍为 `main@50334d31e3f65936e1936c052078e0151780a1c8`；候选源码为包含未提交 P033–P035 修改的 dirty 工作树，不表示该提交已包含这些改动。本候选不是公开发行。

## 本次修正

P034 在实际长挂机使用中出现严重生产回归，已撤回长挂机验收建议。旧 P034 ZIP、说明和源码快照保留作历史证据，不要用 P034 包进行长挂机验收。P035 修正两处已确认的生产路径问题：

- Qt queued SET 携带 `PttAdmissionBlock::Token`，但缺少与 MOC 生成签名完全一致的类型别名注册，导致 `AutoConnection` 无法将 SET 入队。生产注册入口和 MetaDataRegistry 现在共用精确的类型注册；永久 Qt MOC/QThread 软件回归沿用该生产入口。
- Hamlib 成功的 PTT GET 曾更新 readback，却漏报 health 成功结果，令观察状态仍显示 unknown/suspect。现在只在 GET 成功、protocol generation guard 通过并确认 readback 后报告成功；失败和协议错序路径仍保持失败语义。

## 使用边界与验证

Qt queued-call fake 回归证明所测生产注册路径和 worker 队列的消息传递行为；Hamlib 策略/源码回归检查证明成功 GET 的 health 报告接线。它们不证明实际电台收到 PTT、串口上的真实往返，也不证明用户报告的 15 秒周期已恢复。long-idle COM 释放根因仍未验证。P035 仍是本机 review candidate；构建、测试、安装和包校验结果属于本机交付记录，不应扩大 CAT/PTT/TX 或 HIL 验收结论。

CAT health observer 不自动重连、不关闭串口或控制 TX；5 秒仅是未完成长 poll 的观察性 suspect 标记，不是 PTT readiness 超时。Hamlib `RIG_OK` 加 protocol guard 不证明串口/电台实际往返。`readFreq` 仍先调用既有 `sync_transceiver(true)`，再独立请求只读 GET；旧同步调用保留 `enforce_mode_and_split=false` 默认行为及可能的 split-off/TX 频率清零兼容副作用。OmniRig 在 Release 配置中关闭，仅作静态路径审阅。

未启动 JTDX、未连接真实 CAT/PTT/TX、未做 HIL，也未在本机正式安装目录运行；自动化测试不等于完整 GUI、现场验收或射频验证。任何后续实机验收应先验证短时正常收发周期及 PTT 状态，再逐步进行 long-idle 观察；不能根据本候选的软件检查直接认定可长挂机。
