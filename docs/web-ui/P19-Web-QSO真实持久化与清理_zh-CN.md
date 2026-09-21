# P19 Web QSO 真实持久化与清理（2026-09-21）

## 结果摘要

本批基于 `b5763e8`，最终源码提交为 `0033f9a`。P19 只修复 Web QSO 持久化成功条件，并增加隔离的真实 `LogQSO` 文件测试；未扩大 API、未改变 CAT/PTT/TX/UDP/布局方向。

发现并修复两处具体问题：

1. `LogQSO::accept()` 在 ADIF 或 `wsjtx.log` 打开失败后仍继续发出成功信号；Web 提交流程因此可能清空草稿并报告完成。
2. `initWebLogQSO()` 为抑制桌面窗口而设置的标志会让初始化路径误走 `accept()`，随后显式确认再次写入，导致一次确认产生两条 ADIF。

现在 Web 初始化只填充草稿；显式确认必须实际追加 ADIF 和业务日志，失败保留草稿并返回写入原因，成功后才关闭草稿并推进 generation。

## 隔离真实 LogQSO 证据

新增 `logqso_persistence_test`，通过实际 `LogQSO::initWebLogQSO()` 与 `acceptWebQSO()` 写入 Qt test data 目录：

`C:\Users\cassi\AppData\Local\qttest\JTDX-P19-LogQSO-Test`

该目录是唯一测试应用目录，测试开始清空、结束回收；未使用用户 JTDX 配置或用户 `wsjtx_log.adi`。最终输出包含：

```text
P19_LOGQSO_ADIF_RECORDS=1
P19_LOGQSO_FAILURE_REASON=Cannot open file "...\wsjtx_log.adi".
TEST_EXIT=0
```

覆盖结果：

- 显式确认：ADIF 实际存在且只有一条 `<eor>`，`wsjtx.log` 有对应业务记录。
- 取消路径：不调用确认，ADIF/业务日志均为零字节或不存在。
- 不可写路径：将隔离 ADIF 路径替换为目录，真实 `QFile::open()` 失败；返回失败原因，业务日志没有成功记录。
- request_id 重试、当前 DX/时间/频率变化拒绝和无效字段拒绝：继续由 Web Control/Server 契约覆盖；真实主程序本轮只在无当前 DX 条件下执行拒绝验证，未伪造成功确认。

## 真实主程序本地 Web 验证

使用独立 `--test-mode --rig-name P19-Web-Persist-9677`、`Rig=None`、loopback `49233` 启动重新构建的 `jtdx.exe`。证据：

`C:\JTDX64\deps-webui\p19-real-web-20260921-9677.log`

实际状态为 `HTTP=200`、`DX_CALL=`、`ADIF=False`、`LOG=False`。真实 HTTP 拒绝结果：

- 未提供确认：`400 / confirmation_required`
- 未知 QSO 字段：`400 / unknown_qso_field`
- 非法频率：`400 / invalid_qso_frequency`
- 无当前 DX 的打开/取消：`409 / tx_path_active`
- 以上过程最终 `ADIF_EXISTS=False LOG_EXISTS=False`

没有通过 UDP、CAT 或硬件注入 decode/DX，因此真实 MainWindow 的成功 Web 确认、同 `request_id` 重试幂等和确认前 DX/时间/频率变化仍保留为人工/进一步隔离验证项；本批不把 fixture 或无 DX 拒绝当作真实记录成功。

## 构建与回归

- 受影响目标 `jtdx`、`logqso_persistence_test`、`mainwindow_web_frequency_contract_test` 构建成功：`C:\JTDX64\deps-webui\p19-final-build-0033f9a-20260921.log`
- 全量 CTest：`24/24`、`100% tests passed`、总计约 `36.36 sec`：`C:\JTDX64\deps-webui\p19-final-ctest-0033f9a-20260921.log`
- 构建/运行使用本机 MinGW/Qt 路径；未修改系统 PATH。

## P19 RC 交付证据

- 安装目录：`C:\JTDX64\deps-webui\p19-install-20260921-0033f9a`
- 解压目录：`C:\JTDX64\deps-webui\p19-rc-extract-2.2.159.2.10-0033f9a`
- ZIP：`C:\JTDX64\deps-webui\JTDX-2.2.159.2.10-rc-local-0033f9a-p19.zip`
- SHA256：`d334cba188e68e60fadc57f2b1b05158bbb2b7f81740459a101d76759ecf9455`
- 安装目录与解压目录均为 `75` 个文件，直接根目录为 `bin/plugins/share`，递归 SHA 比较 `HASH_DIFFERENCES=0`。
- 两个 RC 目录中的 `bin\\jtdx.exe` 均以 `--test-mode` 启动并保持运行至少 5 秒，随后由本轮拥有的进程 PID 受控停止；这只是无硬件启动烟测，不是正常窗口关闭或 HIL。

## 清理与保留

保留最终 P19 RC 的安装/解压目录、ZIP/SHA256、P19 构建/CTest/真实 Web 证据和 P17/P18 可回退 RC；删除本批明确生成且可再生的临时运行脚本 `C:\JTDX64\deps-webui\p19-real-web.ps1`、编译器临时目录 `C:\JTDX64\deps-webui\p19-compiler-temp`，以及两个独立测试目录 `C:\Users\cassi\AppData\Local\qttest\JTDX - P19-Web-Persist-9677 - test` 和 `C:\Users\cassi\AppData\Local\qttest\JTDX-P19-LogQSO-Test`。保留对应真实 Web 日志，不删除源代码、活动构建目录、依赖目录、用户安装目录或唯一截图证据。

## 未完成边界

真实 CAT/PTT/TX、音频、外部 UDP、LAN/生产部署、正常 MainWindow 人工窗口操作和 HIL 仍未验证；未 push/tag/amend/reset。
