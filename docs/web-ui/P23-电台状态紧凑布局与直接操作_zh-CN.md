# P23 电台状态紧凑布局与直接操作

日期：2026-09-23。范围是 JTDX 内置 Web UI、它的本机 HTTP 状态/控制映射、设置项、自动化合同和本地 Release 候选。仅限源码、隔离软件测试与 loopback 浏览器夹具；不触及真实电台、CAT/PTT/TX、UDP 控制流或公开部署。

## 行为映射

| 用户可见项/动作 | P23 映射 | 必须保持的结果语义 |
|---|---|---|
| 页面状态 | 桌面双栏：最近解码在左，电台状态/频率/DX/操作在右；窄屏纵向排列。RX 与运行字段合为一张状态卡；移除新鲜度/陈旧年龄、快照年龄和独立服务/操作快照卡。 | 不把内部 revision、freshness 或服务状态字段作为 UI 控件门槛。未知业务/电台状态仍不会伪报成功。 |
| 周期进度 | 读取 MainWindow 的 JTDX 校正时钟 `currentMSecsSinceEpoch2()` 与 `m_TRperiod`（秒），向状态 API 提供 `jtdx_time_ms`、`cycle_period_ms`。浏览器用状态请求往返中点校准到 `performance.now()`，按周期计算进度。 | 首次连接、重连/恢复、模式周期变化、页面重新可见时重新校准；周期动画不依赖 `Date.now() % 15`，也不以浏览器墙钟代表 JTDX 校正时间。 |
| 频率 | 当前电台频率与候选预设放在频率卡；目标是整数 Hz，初值跟随实际频率，用户编辑中不被轮询覆写；候选只填入，不会自动发送。 | 完成仍取决于 CAT 实际频率回读；状态未知、TX/PTT 活动或回读不匹配不能报告完成。 |
| DX | 呼号/网格始终是可编辑字段，空值保持为空；支持手动提交，也可从最近解码选择。轮询不覆盖用户正在编辑的值。 | 结果按 manual/decode 来源与对应回读核对；保留业务/电台安全检查和真实结果，不因不存在 decode ID 而拒绝手动输入。 |
| Web 操作 | 移除 Web 专属功能能力复选框、通用二次确认及请求依赖的快照 revision、来源新鲜度、Host/Origin/CSRF 限制。 | 原生业务入口的发射/停止安全边界、未知状态 fail-closed、幂等、epoch/在途超时语义与匹配回读保持有效；QSO 草稿仍要求用户明确提交，因为那是业务写入动作而非通用 Web 确认门。 |
| Web 启停/监听 | 没有存储偏好时默认启用；显式禁用持久化。默认监听 loopback，配置拒绝通配 IPv4/IPv6 地址。 | “默认启用”不等于 LAN/公网授权；不自动改变用户绑定偏好，不开放通配地址。 |

## 代码与门控映射

- `Configuration`/`Configuration.ui`：删除 LAN/能力 gate 项，保留 Web 启用、绑定地址与端口。初次缺少偏好时启用，显式禁用保存；端口校正不会反向改写启用偏好。
- `JtdxWebServer`/`JtdxWebControl`/`JtdxWebService`：移除 Web 专属 capability、Host/Origin/CSRF、confirm 和状态快照版本拒绝；频率、DX、业务和电台操作仍走现有 dispatcher、在途/幂等状态与业务/CAT 回读。通配监听继续拒绝。
- `JtdxWebState`/`MainWindow`：不再对 UI 输出 freshness/age/生成时间等状态字段；删去依赖这些字段的频率结果完成条件，维持线上 CAT observation 与真实频率回读。周期钟通过窄回调接入 JTDX 校正时间和 `m_TRperiod`。
- `resources/web-ui`：简化页面结构和状态呈现、实现编辑保护和手动/解码 DX 双路径、整数 Hz 与周期同步。窄屏解码行将主要字段与消息分行呈现。
- 测试更新覆盖默认启用/显式禁用设置、状态字段删减、校正时钟/周期毫秒、无需 Web 快照年龄的普通控制、手动 DX、路由与 UI 合同。

## 验证结果

- 配置：CMake `MinGW Makefiles`、MSYS2 MinGW-w64 Qt5，Release；完整 `jtdx` 与全部本地测试目标构建退出码为 0。构建目录为 `C:\JTDX64\build-webui-dev-msys2`。最终源码提交、完整 CTest 次数/耗时及安装记录将在末尾补齐。
- 全量测试：最终提交后重新运行 CTest；覆盖 24 项。以运行记录的最终结果为准。
- 前端：`node --check resources/web-ui/app.js` 与 `git diff --check`。
- 浏览器：使用 `jtdx_web_server_test.exe --serve-browser-automation-p9` 本机 loopback 隔离夹具（只构造 State/Control/Server 内存状态；不创建 MainWindow、CAT、PTT、音频或 UDP）。桌面 1280×720 实测 workbench 1064 px、两栏各 525 px、根横向滚动宽度与 client 宽度均 1265 px；窄屏 390×844 实测可视宽度 390、根宽/scrollWidth 均 375 px，左/右工作区纵向排列。夹具还提供测试周期时钟以检查周期条显示；不是实机校时或实际主窗口视觉证明。隔离控制仅用于验证页面请求/回读，不是电台操作。
- 本地安装树/ZIP 的文件数、SHA-256、根目录和清洁解压校验会在候选包完成后补记。P22 既有 staging、ZIP、来源材料和回退文件均保留，不覆盖。

## 交付与未验事项

本批只产出本机审阅候选，不推送、不打 tag、不公开部署，也不声明满足公开再分发或法律义务。P22 已核对的许可证/Notice 材料会随运行时保留；逐文件核验尚未证明 `ALLCALL7.TXT`、`CALL3.TXT`、`JPLEPH` 的来源和再分发许可，`cty.dat` 随附版权字段不完整，Qt LGPL/间接 DLL 对应源码与具体分发义务也仍需审查。包内有许可证文本不等于义务已完成。

未验证：真实 MainWindow 手工设置/保存重载/完整 GUI；真实 CAT/PTT/TX、实机周期校准、硬件回读、HIL；局域网/公网访问；部署、发布审批与完整 QSO 主窗口写入集成。隔离测试不替代这些证据。
