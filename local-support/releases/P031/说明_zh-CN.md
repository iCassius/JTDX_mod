# P031 本机测试包交付说明

## 版本与结果提交

- 产品版本：2.2.159.031；Windows PE 数字版本：2.2.159.31。
- 构建来源结果提交：b187262f86914b68673344e781fd6e034aa3c621，分支 main。
- 版本提交包含 .031 显示/资源版本、版本标题自动化断言和 Web 版本展示夹具更新，以及本批用户行为说明。交付证据与打包校验规则由后续中文交付提交补齐；ZIP 名称保留其构建来源提交短哈希 b187262。

## 对使用者的影响

- AutoSeq 回答 CQ 时尝试计数会持续累计；达到启用的最大次数后必定进入终止清理，避免 DX 被旧目标长期占用。方向 CQ 只决定能否选为候选，不再重置已经累积的次数。
- 自动特殊目标达到上限后进入五分钟进程内冷却，持续发送普通 RCQ/RFIN 的同一台不会立刻重新占住 DX。明确呼叫本台的 RCALL/RREPORT/RRREPORT 和有效 QSO 回应 RRR/RRR73 可继续；空/非法报告按最弱报告 -60 处理。冷却不写入持久配置。
- CAT 运行会话故障不再因 Configuration 窗口打开而只显示本地提示，会进入 MainWindow 既有恢复流程。配置测试会话故障只在设置页提示；Test PTT 复用运行 rig 时保留运行身份；配置接受/取消后按最终 rig 归属设定身份；旧 generation 的迟到故障会被丢弃。
- 现有 PTT-on 保护、重连次数、恢复票据和 online/PTT-off 回读门槛保持不变；没有新增自动发射动作。
- TCP 生产端口选择、UDP 与 Web UI 行为未改。只把自动端口冲突测试夹具改为在 AnyIPv4 上占用候选端口，以验证跳过占用端口与数字 UDP 保留端口。

## Release 配置与构建

- 独立构建目录：local-support/build/P031-release；生成器 MinGW Makefiles；Release；GCC 16.1.0；JTDX_BUILD_LOCAL_TESTS=ON；WSJT_ENABLE_OMNIRIG=OFF；WSJT_HAMLIB_TRACE=OFF。
- 使用 MSYS2 MinGW64/Qt 工具链及 Hamlib 4.7.2 SDK。环境缺少可选 asciidoctor，因此按项目 CMake 提示设置 WSJT_GENERATE_DOCS=OFF；JTDX 主程序和所有本地测试目标均完整构建。
- 完整 all 构建退出码 0；最终全量 CTest 32/32 通过，用时 57.73 秒；P031 版本标题定向测试 1/1 通过。构建包含既有 unused-variable、弃用 API 和 Fortran 警告，无构建错误。此前的配置/版本断言诊断日志保留在 evidence，最终通过记录以 configure.log、final-build.log、final-ctest.log 为准。
- 全量测试使用 QT_QPA_PLATFORM=offscreen 与 MSYS2 Qt platform 插件。自动化测试不能替代 GUI、真实 CAT/PTT/TX 或 HIL 验收。

## Hamlib 与运行依赖核验

- Hamlib 官方 Win64 4.7.2 包：C:\JTDX64\deps-webui\temp\P21-hamlib-w64-4.7.2.zip，SHA-256 8553BC6C5C6032E8DEBF99C017E98F58FED7E07E7C25D04815DC3E8BBE3304C7。构建链接 Hamlib 4.7.2 SDK；runtime 来自此官方包解压的 P21-hamlib-official-extract/P21-runtime-stage，未从旧 C:\JTDX64\159 目录取 DLL。
- 包内 bin/libhamlib-4.dll 为 11,981,824 bytes，SHA-256 72F09E0BC1118C11AE0652612D58BD04BA620C4DCF2D4181D8DC39D4AF60D28E，与官方发行包解压件匹配。rigctl-jtdx.exe --version 输出 Hamlib 4.7.2。rigctlcom-jtdx.exe、rigctld-jtdx.exe 与对应 P21 官方 runtime 的哈希也匹配。
- 安装日志中 CMake fixup_bundle 验证 7 个 EXE。对暂存 49 个 EXE/DLL 的 277 个静态导入项扫描显示：120 项由包内文件提供，157 项由 Windows System32 提供，缺失 0。该扫描只核对静态导入闭包，不证明运行时动态加载的所有插件路径。
- bin/jtdx.exe PE FileVersion 为 2.2.159.31、ProductVersion 为 2.2.159.31 b18726，文件 27,445,455 bytes，SHA-256 8AC1657ED6717BE1921C7E6EFDBB9D2EA110324EE6FE8D708F6D3D5836737780。只读取文件资源，没有启动该程序。

## ZIP 与根目录核验

- ZIP：C:\JTDX64\JTDX-2.2.159.031-local-b187262-P031.zip
- 大小：49,402,089 bytes；SHA-256：95274274CE774F929FE85D86422ED961B61BD989297D3B57892F6177FEA6B08E
- 共 142 个文件，未压缩内容 137,604,364 bytes；根项恰好为 bin/、plugins/、share/。无根级说明、脚本、NOTICE 或其他文件；NOTICE 文本放在 share/doc/JTDX/。
- manifest-P031.csv 记录每个相对路径、长度和 SHA-256。暂存、ZIP 与清洁解压均逐项 142/142 匹配；重新计算 ZIP SHA-256 和 -ValidateOnly 均通过。所有 ZIP 条目均无绝对路径、反斜杠或 ./.. 段。
- C:\JTDX64 本批只新增上述一个 ZIP；未创建 .sha256 sidecar。P029/P030 ZIP 的 SHA-256 与打包前快照一致。打包校验 helper 现在只接受 bin、plugins、share 三个顶层目录，并拒绝绝对路径和 traversal 路径。

## 证据文件

- evidence/P031/source-commit.txt、configure-command.txt、configure.log
- evidence/P031/clean-before-final-build.log、final-build.log、final-ctest.log、version-title-focused-ctest.log
- evidence/P031/install.log、jtdx-version-hash.log、rigctl-version.log
- evidence/P031/hamlib-provenance.log、dependency-import-audit.csv、dependency-audit-summary.log
- evidence/P031/stage-seed.log、stage-root-policy.log、manifest-P031.csv、manifest-tree-verification.log
- evidence/P031/package-run.log、package-verify.log、package-validate-only.log、release-root-before.csv、release-root-after.csv、release-root-diff.log

## 用户测试建议与未验证项

1. 解压到独立目录，保留原有 JTDX 安装及配置；先用接收/安全配置确认版本显示为 2.2.159.031。
2. 观察 AutoSeq 回答 CQ 的计数是否累计；到达配置上限后 DX 是否释放；五分钟冷却内持续的普通 RCQ/RFIN 是否不会立刻重新占用，同时定向呼叫和正常回应是否继续。
3. 在不发射的安全条件下检查 Configuration 开/关时运行 rig 故障路由、Test CAT 本地错误提示、Test PTT 复用运行 rig 的身份，以及 generation/session_purpose 诊断日志。真实故障恢复需结合用户自己的 CAT 设备回读判断。
4. 需要发射相关验证时由用户自行选择设备与安全操作条件；本候选自动化结果没有验证无线电实际行为。

本次没有启动包内 jtdx.exe，没有连接/操作真实 CAT/PTT/TX，没有发射，没有 HIL、LAN/浏览器人工验收或网络部署。此 ZIP 是供本机用户测试的候选包，不表示公开发行批准，也不构成 Qt/第三方依赖或数据文件再分发义务已完成的法律结论。
