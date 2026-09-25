# P030：IPv4 默认监听与访问地址

## 结果

- 源码提交：`6283cac280f568c8681c5aa032157c45c63877c0`，分支 `main`；应用显示版本 `2.2.159.030`，Windows PE 数字版本 `2.2.159.30`，产品版本 `2.2.159.30 6283ca`。
- 完整 Release 构建成功，最终全量 CTest `30/30` 通过（57.47 秒）。构建标题时间为 `2026-09-25 12:12:34 UTC`。
- 本机审阅 ZIP：`C:\JTDX64\JTDX-2.2.159.030-local-6283cac-P030.zip`；大小 `49,373,063` bytes，SHA-256 `74F2085ADC3E269974D24604343C1D186F150AF883B47B8F5BE4F2806010B454`。包含 142 个文件，未压缩内容 `137,574,291` bytes；ZIP 直接根项为 `bin/`、`plugins/`、`share/`、`NOTICE`、`NOTICE_zh-CN.md`。ZIP 对暂存及清洁解压逐项大小/SHA-256 `142/142` 一致。
- `C:\JTDX64` 本批唯一新增项是上述 ZIP；没有 `.sha256` sidecar。哈希、manifest、暂存、清洁解压、日志和说明均在源码仓库 `local-support`。
- 候选只供本机审阅，不代表用户验收或公开发行批准。没有启动包内的 `jtdx.exe`、没有操作 CAT/PTT/TX，也没有做 LAN 客户端、真实浏览器访问或 HIL 验收。

## 行为与迁移

- 没有 `WebUiBindAddress` 的新配置现在默认 `0.0.0.0`，即监听所有 IPv4 本地接口；显式 `0.0.0.0` 也有效。仍拒绝 IPv6 通配地址 `::`。
- 已存配置一律保留原值，不自动覆写。历史版本会把默认 `127.0.0.1` 一并写入配置，无法区分它是旧默认还是用户明确选择的“仅本机”；因此升级后已有 `127.0.0.1` 仍保持仅本机。升级用户若需 LAN 监听，必须自行将 `WebUiBindAddress=0.0.0.0`（或在设置页填写并保存 `0.0.0.0`），然后重启 Web 服务；程序不静默改写配置。没有存储该键的新配置默认通配监听。
- Windows 访问 URL 从 IPv4 默认路由表选择：按 Windows 的 route metric + interface metric 排序；只取接口 Up/Running 的有效单播 IPv4，排除 loopback、169.254/16、未指定、广播及组播地址。并列时按接口索引稳定排序。无可用路由地址时 URL 回退 `127.0.0.1`，监听仍可在 `0.0.0.0` 上成功。
- 对非 Windows 构建，当前没有实现平台专属的默认路由表查询，故安全回退为 `127.0.0.1`，不靠接口枚举顺序猜测默认出口。
- URL 使用成功监听后缓存的可访问地址和实际 TCP 端口；设置页面、状态栏/菜单提示与菜单打开动作共用同一 URL。显式绑定具体地址（例如 `127.0.0.1`）时，访问 URL 仍为该地址。服务重启会重新选择默认路由地址。

## 本机只读快照与验收边界

- 2026-09-25 对既有 JTDX 只读查看：PID `26100`，`C:\JTDX64\159\bin\jtdx.exe`，仅监听 `127.0.0.1:49152`。这不是 P030 构建，也未对其作任何操作。预期的两个 AppData `JTDX.ini` 路径均不存在，不能据此断言该进程的持久设置。
- 同日 Windows IPv4 默认路由只读读数：接口索引 `20`（以太网），route metric `0`、interface metric `15`、下一跳 `192.168.50.233`、本机 IPv4 `192.168.50.105`、状态 Alive。该地址是新行为在当时路由快照下的预期 URL 主机；并非通过启动 P030 实测。
- 自动化用例覆盖合成的多路由/metric、VPN/虚拟接口优先级、并列稳定次序、断开接口、loopback/APIPA/无地址过滤和 fallback；TCP 测试短暂启动了无控制能力的 IPv4 wildcard listener，并检查 URL 不泄露 `0.0.0.0` 且端口准确。较长的控制/HTTP 集成夹具仍显式限制为 loopback。
- 从既有 P029 完整 runtime/license 暂存派生 P030 隔离暂存，再用 P030 build 执行 CMake install（退出码 0）。相对 P029 的 142 项文件树中，138 项 SHA-256/大小相同，4 个重建 EXE 更新（`jtdx.exe`、`jtdxjt9.exe`、`udp_daemon_jtdx.exe`、`wsprd_jtdx.exe`）；其余运行库、插件、数据和许可证文件保持匹配。CMake bundle 检查报告 7 个 exe 有效；另用 `objdump` 扫描暂存中的 49 个 EXE/DLL、277 个导入项，120 项由包内文件提供、157 项由 Windows System32 提供，未解析项为 0。该依赖核验不是第三方许可/再分发义务的法律结论。
- 打包脚本的 PowerShell AST 解析通过；旧 P029 ZIP 只读验证 `142/142` 通过；P030 在没有 sidecar 的情况下 `-ValidateOnly` 通过；对现有 P030 ZIP 的覆盖尝试被脚本拒绝。根目录打包前后快照确认只新增 ZIP，sidecar 不存在。
- 未验证：Windows GUI 中手动保存/重载操作、真实新程序监听/地址切换、其他真实 VPN/多网卡拓扑、LAN 客户端访问、防火墙策略、网络部署、HIL。没有更改 UDP、路由、防火墙或系统网络配置；旧的 `XXXXXXXXX` 解码显示问题依用户最新指示跳过，本批未检查/修复。

## 证据

- 配置：`local-support/evidence/P030/configure.log`
- 最终提交版构建：`local-support/evidence/P030/final-build.log`
- 最终提交版全量测试：`local-support/evidence/P030/final-ctest.log`
- 安装与运行库导入审计：`local-support/evidence/P030/install.log`、`P029-P030-stage-comparison.csv`、`dependency-import-audit.csv`
- 打包脚本回归：`local-support/evidence/P030/script-validate-existing-P029.log`、`package-validate-only.log`、`package-overwrite-refusal.log`
- 包完整性与根目录边界：`local-support/evidence/P030/manifest-P030.csv`、`package-run.log`、`package-verify.log`、`release-root-before.csv`、`release-root-after.csv`
- 最终 CTest 记录 30 项全通过、总用时 57.47 秒；构建存在一条既有的 `Configuration.cpp` `extra_items` 未使用编译 warning，不影响构建/测试结果。CMake install 输出若干 `resolved_item == resolved_embedded_item - not copying` 提示（同名依赖已在 stage），随后完成 `fixup_bundle` 并验证 7 个可执行文件。

## 人工验收建议

1. 在隔离配置、无 CAT/PTT/TX 操作的环境运行新构建；确认全新/无键配置监听 `0.0.0.0`，访问 URL 显示活动默认路由 IPv4 + 实际端口，而不是 `0.0.0.0`。
2. 切换默认路由或 VPN 后点“重启服务”，确认设置显示、状态/菜单提示和“打开 URL”仍一致，并且主机地址按新路由更新。
3. 显式设为 `127.0.0.1`，确认只显示本机 URL；断网或无可用 IPv4 默认路由时，通配监听仍可启动、URL 回退 localhost。
4. 验收 LAN 访问前另行审查 Web UI 的访问控制及本机防火墙规则；本次既没有开启防火墙端口，也没有证明其他设备能访问。
