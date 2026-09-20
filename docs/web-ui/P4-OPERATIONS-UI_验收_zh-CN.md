# P4 操作结果卡片验收记录

## 本轮结论（2026-09-20）

隔离 `--serve-browser` 真实 TCP fixture 的浏览器验收已完成。fixture 仅运行 5 分钟、使用只读数据，生产 `frequency gate=false`，并模拟 4 条操作结果；本轮只是补写上一轮已完成的证据，不新增测试。构建与全量 CTest 的权威日志分别为 `C:\JTDX64\deps-webui\p4-operations-ui-final-build.log` 和 `C:\JTDX64\deps-webui\p4-operations-ui-final-ctest.log`：构建退出码 `0`，CTest `19/19` 通过，总耗时 `57.20 sec`。

浏览器验收观察到：`pending`、`timeout`、`failed`、`completed` 的中文状态正确；仅 confirmed completed 行显示 `14.075000 MHz`，其他频率值为未知，顶部实际频率仍为未知；陈旧快照提示正确。`1280x900` 宽屏和 `390x844` 窄屏视觉通过，窄屏 `clientWidth=scrollWidth=375`，无水平溢出。停止自建 fixture 后页面显示“连接断开，保留旧操作快照”，四条结果仍保留。

本轮未启动真实 JTDX，未连接 CAT，未执行 PTT/TX/HIL。上面结果不覆盖 20 条显示上限、`server_epoch` 切换、异常字段浏览器场景或自动化 DOM 断言；这些仍是静态实现待专项。

## 本片范围

本片只为既有只读 Web 页面增加操作结果展示，使用 SSE `snapshot` 中的 `state.operations`。页面读取最多 128 条结果，按接收顺序仅显示最近 20 条，并使用 DOM `textContent` 写入文本；没有新增 POST、频率表单、DX/CQ/AutoSeq 按钮，也不保存令牌。

涉及文件：

- `resources/web-ui/index.html`：增加“操作结果”只读卡片和说明。
- `resources/web-ui/app.js`：增加有界结果缓存、状态/回读映射、服务 epoch 切换和陈旧快照提示。
- `resources/web-ui/style.css`：增加深色卡片、状态标签和移动布局。

## 页面行为

- `accepted`/`pending` 只显示“处理中”；`completed` 只有 `readback.confirmed === true` 才显示“已完成（已确认）”。完成但未确认、未知状态或异常字段显示“未知”。
- 频率确认只读取当前操作自身的 confirmed readback，且只对频率操作显示；Hz 转 MHz 后显示 6 位小数。它不会回写或替代页面上方的当前实际频率。
- 操作耗时只显示 `completed_ms - received_ms` 的单调毫秒差值；缺失、非法或仍未完成时显示“未知”，不把这些字段当作日期。
- 收到不同 `server_epoch` 时清空旧操作缓存，并只使用新快照中与当前 epoch 相同的结果。缺少 epoch 时不显示旧结果。
- `operations` 缺失或不是数组时显示“未知”；快照 `freshness` 不是明确的 `fresh`/`stale` 时也显示新鲜度未知，不把未知状态当作新鲜。
- 连接断开或超过 15 秒没有新快照时，保留旧结果但将卡片突出为陈旧/旧快照；未取得过快照时显示未知。业务状态字段和业务 terminal status 不被页面改写。
- 请求 ID、原因和其他文本均通过 `textContent` 写入并设置长度上限；结果行最多处理 128 条，最多生成 20 条可见行。

## 未覆盖专项

以下项目没有被本轮浏览器验收声称覆盖，仍需后续专项：

1. 超过 20 条时页面最多显示 20 条。
2. `server_epoch` 改变后旧行消失并由新快照替代。
3. 异常字段的浏览器展示。
4. 自动化 DOM 断言。
