# P22 本地候选与许可材料

## 结论

P22 是内部审阅候选，不是公开发行包或清权结论。源码 HEAD 为 `50ad747c69c9bb76834f3fe784894ddde86ef0f0`；最终 `jtdx.exe` 产品资源版本为 `2.2.159.2 50ad74`，PE 固定文件版本为 `2.2.159.2`。本轮没有源代码功能改动。

候选 ZIP：`deps-webui/release-current/JTDX-2.2.159.2.10-local-50ad747-P22.zip`。证据目录：`deps-webui/evidence/P22`；构建依赖对应源码材料：`deps-webui/source-materials/P22`。

## Hamlib 与 FTX-1 恢复行为

P22 使用 Hamlib 官方 4.7.2 Win64 二进制重新链接。P22 DLL SHA-256 为 `72F09E0BC1118C11AE0652612D58BD04BA620C4DCF2D4181D8DC39D4AF60D28E`；旧 `C:\JTDX64\159\bin\msys-hamlib-4.dll` SHA-256 为 `630A90E02F56E0D5F02A77E8D172F61F041399900DBFFA57A4FB989896895DB3`。离线 dummy `rigctl` 运行时模块记录证明 P22 测试加载了 staging 中的 DLL，而不是旧目录 DLL。

两 DLL 的 560 个唯一导出名称集合相同，FTX-1 model 1051 的显示行及 backend 字符串 `20251224.0` 相同；这不是行为等价证明。旧 DLL 缺少可用的构建元数据，旧二进制对应的上游提交、本地补丁和完整构建配方未能恢复。因此不得声称官方 DLL 与旧 DLL 完全等价。

跟踪到的 `328cc7a` 修复改动位于 JTDX 的 `HamlibTransceiver` 调用层及 FTX-1 CAT poll policy：通过 Hamlib debug callback 检测 `newcat_get_cmd: wrong reply`，并把轮询代际/`protocol_sync` 状态交给应用侧恢复策略。故这项 CAT 轮询错序恢复修复属于 JTDX，而不是对 Hamlib DLL 的隐含修补。官方 Hamlib 4.7.2 自身也有 FTX-1 后端更新，但不能据此推导旧二进制行为完全复现。

## 构建与测试

- CMake Release / Ninja / Windows x64；最终构建、安装退出码 0。
- 最终 staging 定向 CTest：11/11 通过；完整 CTest：24/24 通过。LogQSO persistence 与 Web server/service 测试均通过。证据均位于 `deps-webui/evidence/P22`。
- CTest 运行时 PATH 仅包含 P22 staging `bin` 和 Windows 系统目录；Qt offscreen 测试插件来自独立临时目录，不打入候选包。
- `udp_mirror_test` 只将套接字绑定到 `127.0.0.1` 随机端口；未使用 UDP 2237，也未连接 GridTracker2。
- 离线 Hamlib 工具检查成功：`rigctl -l`/`--version` 显示 4.7.2 与 FTX-1 model 1051；dummy rigctl 实际加载路径与哈希有单独记录。
- 未启动 JTDX GUI，未连接真实电台/硬件，未操作 CAT/PTT/TX；未进行 HIL、长时间运行或生产环境验证。

## 对应源码与授权边界

`deps-webui/source-materials/P22` 收录 JTDX HEAD archive、Hamlib 4.7.2 源码、MSYS2 Qt Base/Multimedia/SerialPort/WebSockets 的精确版本源码包、FFTW 3.3.11-1 与 GCC 16.1.0-5 源码包及 SHA256 清单。构建使用的其他间接运行库（如 ICU、GLib、Freetype/HarfBuzz、libusb 等）并未全部附上其对应源码归档。MSYS2/Qt 上游来源和构建包不自动证明针对实际分发方式已满足完整对应源码、通知、重新链接等义务；Qt LGPL 义务仍须单独核验。

随包 `share/doc/ThirdParty` 带有本机可取得的第三方许可证文本，但文本收录不等于完成法律审查。

随安装内容的 `ALLCALL7.TXT`、`CALL3.TXT`、`JPLEPH` 仍缺少足以核实其确切来源、版本和再分发授权的证据；`cty.dat_copyright.txt` 的主体/年份字段也不完整。因此 P22 只可作本地审阅，不得公开分发。不得仅凭 JPL 星历的通用说明将仓库内具体 `JPLEPH` 判定为公有领域。

## 主要证据文件

- `P22-cmake-configure-50ad747.log`、`P22-cmake-reconfigure-50ad747.log`
- `P22-build-final-50ad747.log`、`P22-build-version-resources-retry-50ad747.log`
- `P22-install-final-50ad747.log`
- `P22-targeted-ctest-finalstage-50ad747.log`、`P22-ctest-final-50ad747.log`
- `P22-actual-rigctl-dummy-modules-50ad747.csv`、`P22-no-hardware-tool-check-50ad747.log`

归档只在本机本地候选目录生成，未 push、未打 tag、未部署。
