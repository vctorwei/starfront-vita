# Starfront: Collision HD · PSV 移植版

[下载 VPK](https://github.com/vctorwei/starfront-vita/releases/tag/v0.0.6) · [English / 完整 Credits](README.md#credits)

**00.06 开发测试版，应用 ID：`SFHP00001`。**

通过兼容层在 PSV 上加载原版 Android ARMv7 游戏库。隔离的 Vita3K 测试已进入战役第一关；完整战役、真机长期稳定性和本版封面更新效果尚未完成验证。

## 已安装旧测试版

在 VitaShell 中覆盖安装 00.06 VPK。原资源和存档保留，PSV 包关闭 `port.log`，继续使用 1024×600 逻辑布局。00.07 新增的启动封面更新逻辑已撤回，整张 LiveArea 页面未更新的问题仍保留。

## 首次安装

1. 准备可运行自制程序的 PSV、VitaShell 和已加载的 [kubridge](https://github.com/bythos14/kubridge)。`libshacccg.suprx` 应位于 `ur0:data/` 或 `ur0:data/external/`；可参阅 [ShaRKBR33D](https://github.com/Rinnegatamante/ShaRKBR33D)。修改内核插件后需要重启。
2. 从自己持有的 Android 1.0.0 APK 提取 `lib/armeabi-v7a/libstarfront.so`，放到 `ux0:data/starfront/libstarfront.so`。本测试版只支持 8,733,105 字节、SHA-256 为以下值的库：

   ```text
   a362b3b46cacad41d3d6c2961b5e4c1dce0027dd4a3c027471652665c275a618
   ```

3. 把完整解包后的 `GloftSFHP` 目录放到 `ux0:data/starfront/GloftSFHP/`，约 1.02 GiB。首次安装时，把 APK 中 `assets/data.save` 分别复制到 `ux0:data/starfront/GloftSFHP/data.save` 和 `ux0:data/starfront/save/files/data.save`。更新时不要覆盖已有存档。原样放入 APK 或 OBB 不能代替解包。
4. 用 ZIP 解压工具打开下载的 VPK，将其中 `compat/GloftSFHP` 合并到 `ux0:data/starfront/GloftSFHP/`，补齐两份地形适配 shader。已有这两份文件的旧测试用户可以跳过。Release 只提供 VPK，不另发补丁 ZIP。
5. 使用 VitaShell 安装 VPK，打开 **Starfront Test**。看到 `Preflight passed` 后按 **×**。00.06 不执行自动 LiveArea 登记，封面页面问题仍待单独修复。

## 操作与问题反馈

前触屏操作菜单和游戏，双指使用原版多点触控；START 对应返回，SELECT + START 退出。× 只用于启动检查页。尚未实现完整按键操作方案。

成就修复写入本地 `androidTrophy.dat`，不提供 PSN 奖杯。音频听感、存档恢复、完整战役、联网及长期真机表现仍需测试；未知 Android/JNI 调用可能停止并显示错误。当前公开包面向 PSV，不能据此保证标准 Vita3K 的兼容性。

遇到问题请在 [Issues](https://github.com/vctorwei/starfront-vita/issues) 提供版本、设备与固件、复现步骤和完整错误照片。PSV 包不写 `port.log`。出现 `C2-12828-1` 时附上系统显示的转储文件名，不要上传 APK、OBB 或游戏资源包。

## 致谢

完整名单见 [README 的 Credits](README.md#credits)，包含 **Backstab Vita、Modern Combat 3 Vita、Modern Combat 2: Black Pegasus Vita、GTA SA Vita** 四个参考项目，以及 MC3 原致谢中的 **TheFloW、Rinnegatamante、CatoTheYounger、Once13One、GrapheneCt**。MC3 的贡献归属按原项目说明，没有将其写成 Starfront 的直接贡献。

本项目独立于 Gameloft 与 Sony。游戏及美术属于原权利人；依赖库各自保留许可。仓库仅发布说明和 VPK，不包含 APK、OBB、原版游戏库、完整资源、存档或调试日志。详见 [许可说明](LICENSE.md) 和 [第三方说明](THIRD_PARTY.md)。
