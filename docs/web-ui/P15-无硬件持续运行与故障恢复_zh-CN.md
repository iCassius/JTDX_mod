# P15 无硬件持续运行与故障恢复证据（2026-09-21）

## 结论

已完成一次 30 分钟以上的无硬件 Web 状态/控制隔离运行：fixture 实际运行 `1802` 秒，自然退出，exit code `0`；脚本结果为 `hard_failure=False`、`unexpected_failures=0`。本证据证明的是现有 Qt State/Control/Server 隔离链路在受限 loopback 客户端、慢 SSE 客户端和安全拒绝/Stop 恢复场景下的有界行为，不是完整 MainWindow、真实 CAT/PTT/TX、无线电、HIL 或公开 Release 稳定性证明。

权威长时日志：`C:\JTDX64\deps-webui\p15-web-fixture-longrun-20260921-r7.log`。

## 本批基线和 RC 边界

P15 为测试专用改动，提交为 `1e5867e`：仅给 `tests/jtdx_web_server_test.cpp` 增加 `--test-duration-ms`，默认仍为 5 分钟，允许范围 `1000..1800000 ms`；不改变生产程序默认行为，不加入 RC 运行包。P15 长时运行使用该提交重新构建的 `jtdx_web_server_test.exe`，不能把它写成 P14 ZIP 已包含该测试参数。

P14 RC 仍是 `main/aef3434`，未刷新、未修改、未重新打包；ZIP SHA256 仍为：

```text
776df727a46242d65a1bdf1908fd86ac98019b39fb9935d7aadd1c28000e70cc
```

P15 构建缓存核对为 `Release`、`Ninja`、`JTDX_BUILD_LOCAL_TESTS=ON`，Qt 来自 `C:\msys64\mingw64`，Hamlib 头文件来自 `C:\JTDX64\deps-webui\hamlib-4.7.2\include`，库为 `libhamlib-4.7.2-from-existing-dll.a`；`WSJT_ENABLE_OMNIRIG=OFF`、`WSJT_HAMLIB_TRACE=OFF`。构建证据：

- `C:\JTDX64\deps-webui\p15-fixture-build-20260921.log`
- `C:\JTDX64\deps-webui\p15-targeted-ctest-20260921.log`

P15 仅对 RC 中的工具做无设备版本读取，没有启动守护服务或连接硬件：

- `rigctl-jtdx.exe --version`：Hamlib `4.7.2`，exit `0`
- `rigctld-jtdx.exe --version`：Hamlib `4.7.2`，exit `0`
- `rigctlcom-jtdx.exe --version`：Version `1.6`，exit `0`

版本读取日志：`C:\JTDX64\deps-webui\p15-rc-tool-version-20260921.log`。动态导入闭包和版本输出只证明文件/工具版本边界，不证明真实设备 ABI、CAT 回读或 PTT/TX 行为。

## 测量计划和隔离边界

| 项目 | 本批设置 |
| --- | --- |
| 进程 | 仅 `C:\JTDX64\build-webui-dev-msys2\jtdx_web_server_test.exe` |
| fixture | `--serve-browser-automation-p9 --test-duration-ms 1800000` |
| 网络 | 仅 `127.0.0.1:49154`；不改 UDP、不连接外部地址 |
| 客户端 | 4 个正常 SSE、4 个不读取 SSE 的慢客户端；前置 16 连接后第 17 个探针验证上限 |
| 读 API | 每分钟 `/api/v1/state` 和 `/api/v1/decodes`，要求 HTTP 200 和可解析 JSON |
| 重连 | 每 180 秒断开并重建 4 个正常 SSE 客户端 |
| 恢复 | 每 120 秒模拟 AutoSeq 启动后 Stop；首次验证 `superseded_by_stop`，状态 stale 后验证安全拒绝仍允许 Stop |
| 采样 | 每分钟采样自有 fixture 的私有内存、工作集、句柄、线程和已建立连接数 |
| 失败标准 | fixture 提前退出、API 非 200/不可解析、连接上限错误、恢复结果不符合安全合同均计为异常 |

前置端口占用验证先让自有监听器占用 `49154`：fixture 以 exit `2` 拒绝启动；释放后同一 fixture 正常监听。未终止任何非本批路径进程。

## 运行结果

- 运行窗口：`1802` 秒，超过 30 分钟门槛；fixture 按测试定时器自然退出，exit `0`。
- 采样：29 次；所有周期状态/解码 API 请求均成功。
- 连接上限：16 个连接后第 17 个探针得到预期 `HTTP/1.1 503 Service Unavailable`。
- 慢客户端：未读取 SSE 的客户端触发有界积压清理；连接数由 8 降至 4，随后正常客户端持续重连，没有连接无限累积。
- 首次恢复：`start-auto-call` 为 `202/pending`；Stop 为 `200/completed/already_selected`；旧启动为 `rejected/superseded_by_stop`。
- 后续恢复：fixture 状态超过新鲜度窗口后，普通启动为 `409/rejected/safety_unknown_or_stale`，Stop 仍为 `200/completed/already_selected`。这是安全门拒绝，不是失败；未把 stale 状态伪装成启动成功。
- 共记录 15 次恢复周期；无意外失败。

资源采样汇总：

| 指标 | 最小 | 最大 | 口径 |
| --- | ---: | ---: | --- |
| 私有内存 | 34.77 MB | 39.33 MB | fixture 进程私有字节 |
| 工作集 | 14.52 MB | 19.34 MB | fixture 进程工作集 |
| 句柄 | 200 | 206 | fixture 进程句柄 |
| 线程 | 2 | 5 | fixture 进程线程 |
| 已建立连接 | 4 | 8 | 仅 fixture 所有连接 |

内存和工作集在早期客户端/SSE 建立阶段上升后回落并保持约 35 MB；句柄和线程未持续增长。该结果支持本场景下未观察到明显的单调资源泄漏，但不构成生产压力、完整浏览器或长于本窗口的稳定性证明。

## 自动回归和正常收尾

受影响测试目标已重建；`jtdx_web_server_test` 与 `jtdx_web_service_test` 定向 CTest 为 `2/2`、`100% tests passed`，总耗时约 `34.36 sec`。长时 fixture 通过 `QCoreApplication::quit` 自然退出，不用 kill 伪造正常退出；服务 stop/start 新 epoch、退出期间拒绝控制等既有合同仍由该定向回归覆盖。

本批未做真实 MainWindow 原生 UI 操作，因此不能把 fixture 自然退出等同于完整 JTDX 主窗口退出释放；该项仍需人工验证。

## Release gate 更新

| Gate | 当前结论 |
| --- | --- |
| P14 RC 结构、SHA256、依赖闭包 | 已通过；P15 未改变 RC，SHA256 未变 |
| 无硬件 Web State/Control/Server 30 分钟持续运行 | 已通过有界 fixture 证据 |
| 受限 SSE、慢客户端、连接上限、周期重连 | 已通过本场景证据 |
| 未决命令 Stop 接管、迟到启动不复活、安全 stale 拒绝 | 已通过软件 fixture 证据 |
| RC `jtdx.exe` 完整 MainWindow 30 分钟运行 | 未验 |
| 完整浏览器人工视觉/设置保存重载/退出释放 | 未验 |
| 真实 CAT、设备回读、PTT/TX、无线电行为、HIL | 未授权 |
| 公共 Release、上传、部署、正式 tag、最终批准 | NO-GO |

后续仍需用户另行安排完整 MainWindow 人工、真实设备 HIL 和最终发布批准；P15 不扩大这些授权边界。
