# P21 本地候选与许可材料

## 结论与范围

P21 是仅供本地审阅的 Windows x64 Release 候选，不是公开发布或法律清权结论。候选由 `main` 上源码提交 `46e1b3b`（功能代码）构建；`012da6f` 是其后的文档提交，`jtdx.exe` 产品版本字符串记录为 `2.2.159.2 012da6`。构建重新指向官方 Hamlib 4.7.2 Win64 发行包后完成链接和 CMake 安装。此次没有重跑 CTest，也没有启动 GUI、操作 CAT/PTT/TX 或连接真实电台。

构建/安装证据位于 `C:\JTDX64\deps-webui\evidence`，文件名以 `P21-` 开头。Hamlib 官方源代码归档 SHA256 为 `ae1fcf2dbc80ea0786ea8f047b09399c3f7737d1930442f61a031708ed33e88f`；官方 Win64 二进制归档 SHA256 为 `8553bc6c5c6032e8debf99c017e98f58fed7e07e7c25d04815dc3e8bbe3304c7`。本地构建使用后者中的 Hamlib 4.7.2 DLL、命令行工具和运行时；旧用户安装目录中的 Hamlib 工具及 DLL 不作为来源。

## 包内许可材料

`share/doc/ThirdParty` 收录随运行时对应的 JTDX GPL 文本、cty.dat 随附声明、Hamlib 许可证/说明，以及可从本机 MSYS2 包许可证目录取得的 Qt、FFTW、GCC runtime、ICU 和其他动态依赖许可证文本。构建依赖包版本、PE 导入扫描、SHA256 清单和逐文件清单保存在 `deps-webui/evidence/P21-*`。DLL 的来源判定以实际哈希对应的已安装包版本为准，不以文件名推断。

许可证文本随包提供不等同于完成每项义务审计。Qt LGPL 动态链接、相应版本的完整对应源代码/构建脚本、用户重新链接权利与书面告知仍须结合具体再分发方式复核；GNU GPL 对完整对应源代码或合规书面要约有要求。此包不应在未完成这些再分发义务审查前对外发布。

## 数据文件边界

安装内容包含 `cty.dat`、`ALLCALL7.TXT`、`CALL3.TXT` 和 `JPLEPH`。`cty.dat_copyright.txt` 中有 MIT 风格许可文本，但版权主体/年份字段在文件本身显示为不完整的 `Copyright © 1994-`。目前未取得能逐一证明后三项数据文件来源、版本和再分发授权的随包证据；特别是不能仅凭 NASA/JPL 的科学数据说明推断本仓库内 `JPLEPH` 文件适用某种开放许可。故本地候选保留原文件及随附材料，但这些数据的再分发授权状态标记为“未核实”，不得宣传为已清权。

## 验收边界

- CMake Release 安装成功；已对安装树 PE 导入做静态解析，276 项导入中未发现未解析项。此结果不覆盖运行期动态加载、系统 API、音频设备或硬件行为。
- 候选 ZIP 应以索引中标记的 P21 文件及其 SHA256 sidecar 为准；清洁解压逐文件哈希比较和 ZIP 根目录检查记录在 `deps-webui/evidence/P21-*`。
- 旧 P20、P17、P18 候选及回退包保留在历史目录；不得覆盖或删除。
- UDP 2237 由 GridTracker2 使用；本轮未启动第二个 JTDX GUI。完整 QSO/LogQSO、重试去重、GUI 关闭、CAT/PTT/TX、HIL、局域网访问和部署均未由 P21 验证。

## 参考来源

- Hamlib 官方 4.7.2 发行页及其源代码/Win64 归档：<https://github.com/Hamlib/Hamlib/releases/tag/4.7.2>
- Hamlib 项目许可证说明：<https://github.com/Hamlib/Hamlib/wiki/Hamlib>
- Qt 开源许可义务：<https://www.qt.io/development/open-source-lgpl-obligations>
- Qt 开源许可与源码入口：<https://www.qt.io/development/download-open-source>
- GNU GPL v2：<https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html>
- MSYS2 当前包元数据与对应源码归档入口：<https://packages.msys2.org/>
- JPL 行星/月球星历介绍（仅说明数据背景，不证明本包 `JPLEPH` 文件的授权）：<https://ssd.jpl.nasa.gov/planets/eph_export.html>
- JTDX 源码项目与 GPL 声明：<https://github.com/jtdx-project/jtdx>
