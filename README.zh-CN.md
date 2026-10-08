> **本项目使用 AI 生成。** 感谢所有前人的 Vita 移植项目，你们的工作为 AI 完成本项目提供了重要参考。

# Starfront: Collision HD · PSV 移植版

[**下载 00.06 VPK**](https://github.com/vctorwei/starfront-vita/releases/download/v0.0.6/Starfront-PSV-00.06.vpk) · [Release](https://github.com/vctorwei/starfront-vita/releases/tag/v0.0.6) · [安装教程](#安装教程) · [真机演示](#真机演示) · [English / Credits](README.md#credits)

![Starfront: Collision HD 游戏内高清原始封面](media/cover.png)

Gameloft《Starfront: Collision HD》的 PSV 移植版。需要 **Android 1.0.0 版本 APK 和配套的 1.0.0 OBB 资源**，不支持其他版本。

**00.06 为开发测试版。** 完整战役与长期稳定性仍在测试中，LiveArea 背景可能仍显示旧图。

## 免责声明

Starfront: Collision HD © 2011 Gameloft。游戏、美术、名称和商标归各自权利人所有。本项目为非官方社区移植，未经 Gameloft 或 Sony 制作、授权或认可。

本项目不提供原版 APK、OBB 或游戏执行文件。玩家必须自行持有合法取得的游戏副本。项目作者不支持或鼓励盗版。

## 安装教程

1. 安装 [kubridge](https://github.com/bythos14/kubridge)：将 `kubridge.skprx` 放入 `ur0:tai/`，在 `ur0:tai/config.txt` 中添加以下配置，然后重启：

   ```text
   *KERNEL
   ur0:tai/kubridge.skprx
   ```

2. 确认 `ur0:data/` 中有 `libshacccg.suprx`；没有的话使用 [ShaRKBR33D](https://github.com/Rinnegatamante/ShaRKBR33D) 安装。
3. 准备自己的 **Starfront: Collision HD Android 1.0.0 APK 和配套 OBB**。用 [7-Zip](https://www.7-zip.org/) 等解压工具打开 APK，将 `lib/armeabi-v7a/libstarfront.so` 放到 `ux0:data/starfront/libstarfront.so`。
4. 解包 OBB 资源，将完整 `GloftSFHP` 文件夹复制到 `ux0:data/starfront/GloftSFHP/`。将 APK 中的 `assets/data.save` 分别复制到 `ux0:data/starfront/GloftSFHP/data.save` 和 `ux0:data/starfront/save/files/data.save`。**更新时不要覆盖已有存档。**
5. 下载 [VPK](https://github.com/vctorwei/starfront-vita/releases/tag/v0.0.6)，用解压工具打开，将其中 `compat/GloftSFHP` 的内容复制到 `ux0:data/starfront/GloftSFHP/`。
6. 用 [VitaShell](https://github.com/TheOfficialFloW/VitaShell) 安装 VPK，启动 **Starfront Test**，检查通过后按 **×** 进入游戏。

**已安装旧测试版：** 保留资源与存档，直接覆盖安装 VPK 即可。

## 操作

使用前触屏操作，支持双指框选。**START** 返回，**SELECT + START** 退出。暂不支持完整按键操作。

## 真机演示

16 秒 PSV 真机录像。点击无声动图可打开带声音的完整视频。

[![PSV 触屏操作演示](media/demo.gif)](media/demo.mp4)

[下载 MP4](https://raw.githubusercontent.com/vctorwei/starfront-vita/main/media/demo.mp4)

## 致谢

感谢 Gameloft、TheFloW、Volodymyr Atamanenko、WolffsRoom、Rinnegatamante、CatoTheYounger、Once13One、GrapheneCt，以及所有 Vita 工具与移植项目的贡献者。

参考项目包括 **Backstab Vita、Modern Combat 3 Vita、Modern Combat 2 Vita、GTA SA Vita**。完整名单及链接见 [Credits](README.md#credits)。

## 许可证

项目原创内容使用 [MIT 许可证](LICENSE.md)，第三方组件与游戏素材保留原有许可。

[第三方致谢](THIRD_PARTY.md) · [反馈问题](https://github.com/vctorwei/starfront-vita/issues)
