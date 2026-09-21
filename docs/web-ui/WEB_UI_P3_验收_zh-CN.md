# P3 设置、菜单与只读前端验收

本批基线为 `ed04be6`，范围限于 P3。`JtdxWebServer` 在 JTDX 主 Qt 事件循环内运行；默认配置关闭，不新增进程、常驻线程或 UDP listener，不提供控制 API。

本文件记录历史 P3 版本的配置合同；当前 P7 已移除访问令牌。自动 TCP 端口候选为 P2 既定范围并排除两个已有 UDP 端口；手动端口在设置确认前只读校验，不执行 UDP bind。绑定默认 `127.0.0.1`，LAN 必须填写具体地址。旧 `WebUiTokenSha256` 键不再显示、写回或参与服务签名。

`/`、`/style.css`、`/app.js` 由 Qt Resource 提供。页面使用原生 HTML/CSS/JavaScript，SSE 通过 `fetch` 读取并解析 `snapshot` 事件，保留 `Last-Event-ID`，断线退避并显示未知/未连接状态。所有动态文本使用 `textContent`。

## 已执行验证

- 独立增量构建：`cmake --build C:\JTDX64\build-webui-dev-msys2 --parallel 2`，`jtdx.exe`、`jtdx_web_server_test.exe` 链接成功；权威日志：`C:\JTDX64\deps-webui\build-webui-p3-final.log`。
- 全量 CTest：`ctest --test-dir C:\JTDX64\build-webui-dev-msys2 --output-on-failure`，`100% tests passed out of 15`；权威日志：`C:\JTDX64\deps-webui\ctest-webui-p3-final.log`。
- 浏览器夹具：`jtdx_web_server_test.exe --serve-browser`，loopback 临时服务，演示状态数据，临时令牌仅用于本地浏览器验证；夹具设置有界 120 秒自动退出。父任务最终复验确认 6 条解码按最新优先显示，`<script>` 仅按文字显示，SNR/DF/mode/call/grid/fresh/is_new、AutoSeq/TX 文本、空实际频率和中文陈旧状态可见；390px 宽度无横向溢出，重复连接仅保留一个 TCP 流。断线后的旧快照/陈旧标识由只读代码检查和错误令牌断线观察支持；未在 120 秒自动退出瞬间观察页面，父任务已确认夹具进程不存在。该夹具不启动 JTDX，不连接 CAT/PTT/TX，不代表生产部署。

- 浏览器复验入口：`http://127.0.0.1:49152/#fixture`，令牌由夹具进程输出；fragment 只在浏览器本地显示演示标识，不会进入 HTTP 请求。已用 PowerShell 直接确认 `/` 与 `/app.js` 返回 200，页面脚本通过 `node --check`。
- 动态 `Configuration` QWidget 夹具已完成隔离验收：覆盖默认关闭/loopback、取消不发布临时业务设置、确定后手动端口和令牌摘要持久化、重复打开取消、独立 `QSettings` 磁盘回读、`Rig=None`/CAT 离线以及 `77881` 非法持久端口拒绝并关闭 Web UI。取消时额外出现的 `Configuration/window/geometry` 是既有 `done()` 窗口几何保存行为，已从业务设置比较中单独剥离。测试 deadline 使用有界且可停止的 `QTimer`，未保留固定本机日志路径。
- MainWindow 完整构造仍未动态启动：其构造会启动既有解码子进程、GUI 定时器并排队 CAT 打开，因此不把完整窗口夹具当作安全证据。已将菜单所需的生产 Web 生命周期提取为 `JtdxWebService`，由 `MainWindow` 真实持有、转换 Configuration 快照并由真实 QAction 入口调用；`tests/jtdx_web_service_test.cpp` 在 `QT_QPA_PLATFORM=offscreen` 下通过 Qt-only 动态验收：默认关闭、QAction→生产 `open()`、默认 `QDesktopServices` URL handler 捕获、失败 opener、重复打开保持 URL/epoch/port、不重复启动、停止后新 epoch、占用端口失败及释放后恢复、错误清除、排队 open 在 shutdown 后拒绝、shutdown 后 apply 拒绝和析构释放 TCP 端口。该证据覆盖 Web 菜单业务路径，不等同于完整 MainWindow 人工点击或真实 JTDX 启动。
- 真实 JTDX、CAT/PTT/TX、HIL、部署和长时间浏览器耐久性继续保持未验证。

尚未执行真实 JTDX 隔离启动、CAT/PTT/TX、HIL、部署和长时间浏览器耐久性验证。P4/P5 控制功能不属于本批。浏览器页面的夹具人工复验已由父任务完成；Configuration 动态验收和 Web 菜单业务路径动态验收已完成，完整 MainWindow 窗口人工点击仍未执行。
