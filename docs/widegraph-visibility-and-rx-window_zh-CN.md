# 频谱窗显隐与 Rx Frequency 解码区说明

## 本次频谱窗修复

`WideGraph` 是独立的顶层对话框。此前 MainWindow 构造阶段会打开它，且 `MainWindow::commonActions()` 在 JT9、T10、FT4、FT8、JT65 和 JT9+JT65 模式初始化/切换时再次无条件调用 `show()`。因此用户手动关闭频谱窗后，后续模式复位会再次打开它；`show()` 也可能使该窗口进入前台。

现在将显隐意图保存在 `QSettings` 的 `WideGraph/visible`：缺少旧设置时默认为可见，兼容既有首次启动行为；启动恢复可见窗时使用 `WA_ShowWithoutActivating`，避免为恢复窗口主动请求焦点；用户点击关闭时记为隐藏，MainWindow 退出期间的清理关闭不覆盖该意图。工具菜单的显式“Wide Waterfall”命令沿用原有 `QWidget::show()` 语义，不额外调用 `raise()`/`activateWindow()`，因此不承诺强制激活或还原最小化窗口。`commonActions()` 只更新图表模式/周期，不再改变窗口显隐。频谱窗原有 `geometry` 设置保持不变。

## 解码区域对象与实际入口

| 英文标题 / 简体中文显示 | Qt 对象名 | 用途 |
|---|---|---|
| Band Activity / 波段活动 | `decodedTextBrowser` | 左侧常规解码列表；新解码调用 `DisplayText::displayDecodedText()`，并附带 wanted call/prefix/grid/country 数据。 |
| Rx Frequency / 接收信息 | `decodedTextBrowser2` | 右侧 Rx Frequency 列表。源代码注释明确称它为 “Right (Rx Frequency) window”。 |

`BandActivity`、`RxFrequency` 并非这两个控件的 Qt 对象名。`mainwindow.ui` 中的标题 label 对象分别是 `label_6`、`label_7`；模式重置路径会重设其显示文字。

### 新解码进入右侧列表的门槛

`mainwindow.cpp` 先对新解码检查以下任一条件；命中后才调用右侧 `decodedTextBrowser2->displayDecodedText()`：

| 门槛 | 条件 |
|---|---|
| Rx 频点邻近 | 解码频偏与 WideGraph Rx 频率相差不超过 10 Hz（含边界）。 |
| 我的呼号 | `actionMyCallRXFwindow` 已启用，且已解析呼号是本台呼号。 |
| 内容匹配 | `enableContent()` 开启，解码文本含 `/`，且含配置列表中长度 3–6 字符的一项（源码用 `length() > 2 && < 7`）。 |
| 当前 DX 呼号完成报文 | Enable TX 关闭；报文恰好解析为四个单词；第 3 个单词含当前 DX 呼号 base call，第 4 个含 `73`。源码注释描述其覆盖 73/RR73。 |
| Wanted Call | `actionWantedCallRXFwindow` 已启用，且左侧 `DisplayText` 返回的通知位 `8`（wanted call）已置位。 |

之后右侧 `DisplayText::displayDecodedText()` 仍会按当前配置进行显示筛选。解码器参数 `nagain` 或 `nagainfil` 为 1，或 `m_bypassRxfFilters` 开启时，会置位 Rx-filter bypass；`m_bypassAllFilters` 单独控制 bypass-all。函数末端二者任一为真都会令 `show_line=true`。

### 右侧列表内部的显示筛选

`DisplayText::displayDecodedText()` 的 `show_line` 决策包括：

- “worked but don't show” 及 potential/new-marker 条件；
- 隐藏大陆、国家过滤、呼号过滤；Wanted 呼号/前缀/网格/国家可影响优先级并部分绕过前置隐藏规则，但非 wanted 的匹配呼号仍可被呼号过滤隐藏；
- 隐藏本洲规则；
- 隐藏 free/non-standard 消息；
- 仅显示 CQ、CQ/RRR/73 等配置对远离当前 Rx 频点（>10 Hz）解码的限制；
- 命中 bypass-Rx 或 bypass-all 时强制放行。

New DXCC、new grid 等主要驱动 `priority`、标记、颜色/字体、wanted/通知位及自动呼叫策略；它们本身不是“右侧列表只接收新 DXCC/网格”的总门槛。某些 `worked but don't show`、wanted 和过滤组合仍可能影响具体行是否显示。因此“不是新 DXCC/网格也能出现”符合当前设计；“新”状态是否改变行样式与是否进入右侧是两件事。

### 其他写入右侧的路径

- 手动双击/转抄：`doubleClickOnCall2()` 是 Band Activity（左侧）信号的处理入口，会临时令 `m_decodedText2=true`；右侧 `decodedTextBrowser2` 直接连到 `doubleClickOnCall()`，此时该标志为 false。左侧消息经 `processMessage()` 转抄至右侧时，必须是非 TX 行、未已存在于 `m_QSOText`，并且（不是本台呼号，或未启用“我的呼号也放入 Rx Frequency”）。转抄仍调用 `decodedTextBrowser2->displayDecodedText()`，所以会受其行过滤影响。左侧 Alt+Ctrl 路径用于加入 wanted call。
- 发射回显：两个实际 TX 完成路径调用 `decodedTextBrowser2->displayTransmittedText()`，这是 TX 回显，不经过新解码的频偏门槛。

本说明只描述现有行为；本次未修改消息筛选、优先级或颜色业务规则。

## 验证范围

本次以源码路径、UI 合约测试和独立 Qt 窗口夹具核查显隐决策；夹具调用与生产 MainWindow 相同的显隐策略，不启动真实 JTDX，不操作 CAT/PTT/TX。启动焦点和真实 MainWindow/操作系统窗口管理器的端到端行为尚未由真实应用/HIL 实测；不据此声称硬件或现场验收通过。
