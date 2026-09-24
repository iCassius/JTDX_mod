# SUPERSEDED：P028 `c545085` 旧候选

- 保留文件：`C:\JTDX64\JTDX-2.2.159.028-local-c545085-P028.zip`
- SHA-256：`1D3D517EA704F3047F300A8BBB03C95FE41A3393C4468BC3C502F82A2656A13D`
- 该候选在最终源码全量 CTest 复跑期间触发 `jtdx_web_server_test` 的 `SSE initial snapshot must succeed`。诊断确认服务端 16KiB 瞬时 backpressure 判定误断开仍在传输的大快照。原始失败和详细诊断留在 `local-support/evidence/P028-version028/`。
- 该 ZIP、sidecar 不删除、不覆盖，状态明确为 `SUPERSEDED`，不应再作为当前候选使用。
- 修复后的当前候选：`C:\JTDX64\JTDX-2.2.159.028-local-bc45bb4-P028.zip`；SHA-256 `2771BCB2CB898ECC39A1D1B6796579EF220E14CD90D2B5D9534019FA81715F89`。针对性测试及两次连续全量 CTest 均通过，详见 [`说明_zh-CN.md`](说明_zh-CN.md)。
