JTDX 2.2.159.2.6-test 自动起呼版 By BI7KGD

启动：运行 JTDX-159.2.6.cmd。脚本按自身目录定位 bin，不写死用户目录。
验证：运行 verify-159.2.6.cmd，可检查运行时文件和 Hamlib DLL SHA256。

本包只包含 JTDX 2.2.159.2.6-test 的 Windows 运行时和本版本说明。
FTX-1 CAT 容错仅限 Hamlib model 1051 的安全只读轮询；写命令/PTT 失败仍硬失败。
Hamlib 源码和 DLL 未修改。自动测试不等于真实电台 HIL；真机 CAT/PTT、音频和发射请由用户单独确认。
