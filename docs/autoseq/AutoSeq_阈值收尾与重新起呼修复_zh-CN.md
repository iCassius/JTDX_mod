# AutoSeq阈值收尾与重新起呼修复

当 `autoCallRareTargets` 启用，且当前自动目标命中已配置优先级时，该目标达到尝试阈值后，程序应结束本次自动起呼并清理目标、进入待机。此修复仅针对这一配置的自动目标路径，不适用于所有 AutoSeq CQ retry threshold，也不泛化到 legacy/manual 清理路径。过去，这条限定路径调用 `clearDX()` 时没有标明清理属于内部收尾；只要自动起呼 owner 仍有效，`clearDX()` 就会把它当成人工清除，留下 operator hold。后续解码可以更新候选呼号，但人工停止门会阻止自动 Enable TX，因而看起来一直停在原目标。

在上述限定路径中，当前修复先以非人工取消方式结束旧 stall owner，再在作用域内标记内部 DX 清理，并恢复进入前的标记值。阈值分支标记本批次跳过，避免 SingleShot 计数器在同一次 `process_Auto()` 中再次触发 `autoStopTx()`。旧目标仍按现有 QSO 历史冷却规则处理；新解码批次的其他合格目标仍经过现有 AutoSeq 和人工停止门。此收尾不会自动恢复旧 TX。

诊断日志记录阈值清理前后的目标、operator hold 和 owner 状态，并记录本批次跳过状态。人工 hold 的设置与清除会记录来源；同一候选因 hold 被阻止时按呼号去重，避免每个解码槽重复刷屏。

隔离回归入口为 `p0_mainwindow_autoseq_retry_cleanup_integration`。它通过 `MainWindowP0IntegrationTestAccess` 驱动实际 `process_Auto()`、`haltTx()`、`clearDX()` 和 RX `guiUpdate()`/`stopTx()`/200 ms PTT OFF 定时器；测试使用隔离的 `Rig=None` 配置，不打开 Dummy CAT，不启动解码器、音频输入或调制器。该测试目标不属于默认构建目标，构建后可执行：

```text
cmake --build <build目录> --target p0_mainwindow_integration_test
ctest --test-dir <build目录> -R p0_mainwindow_autoseq_retry_cleanup_integration --output-on-failure
```

源码修改和静态检查不代表该入口已构建或运行，也不构成真实电台、CAT、PTT、RF 或 HIL 验收。

P066 Windows x64 发行包版本为 **2.2.159.066**。本说明限定的是配置自动目标的阈值待机清理行为；软件构建和测试证据适用的边界不变，不代表真实电台、CAT、PTT、RF 或 HIL 验收。
