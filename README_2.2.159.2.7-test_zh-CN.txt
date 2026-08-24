JTDX 2.2.159.2.7-test 自动起呼版 By BI7KGD

启动：运行 JTDX-159.2.7.cmd。脚本按自身目录定位 bin，不写死用户目录。
验证：运行 verify-159.2.7.cmd，可检查依赖闭包、PE 版本、相对路径启动和 Hamlib DLL SHA256。

重要边界：本包是显式 WSJT_ENABLE_OMNIRIG=OFF 的 Hamlib-only 测试包，只用于验证 Hamlib 后端。
FTX-1 始终使用 Hamlib model 1051；本包不包含 OmniRig/ActiveQt wrapper，也不注册 OmniRig Rig 1/2。
普通 OmniRig 用户请使用源码默认 WSJT_ENABLE_OMNIRIG=ON 的完整 Windows 构建；不得安装 OmniRig 作为本包或 FTX-1 修复的运行时依赖。

Hamlib 源码和 DLL 未修改。自动测试不等于真实电台 HIL；真机 CAT/PTT、功率/SWR 表计、音频和发射请由用户单独确认。
