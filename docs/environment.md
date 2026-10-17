# 本机开发环境

检查日期：2026-10-17；制作工具和运行硬件于2026-10-18更新。

| 项目 | 观察结果 |
| --- | --- |
| GPU | NVIDIA GeForce RTX 3060 Ti，8192 MiB显存 |
| CPU | Intel Core i5-13400F，来自实际打包进程的硬件报告 |
| 内存 | 约15.8 GiB |
| Visual Studio Build Tools | 2022 / 17.14.29 |
| 已有MSVC | 工具目录14.44.35207，编译器14.44.35225；编辑器目标实际构建成功，UBT提示非首选版本（首选14.38.33130） |
| 已有Windows SDK | 10.0.22621.0 |
| Epic Games Launcher | 已通过winget安装，安装程序哈希校验通过 |
| Launcher可执行文件 | `C:\Program Files\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe` |
| UE5.6 | 已安装5.6.1，路径 `D:\sdk\UE_5.6`，Changelist 44394996；编辑器目标编译通过 |
| Blender | 4.0.2，`D:\sdk\Blender_4.0\Blender Foundation\Blender 4.0\blender.exe`；从已有、签名有效的安装包提取，已实际生成模型和动画 |
| 安装后空闲磁盘 | D约29.2 GiB（编译缓存生成前）；以实际磁盘查询为准，沙盒下Get-PSDrive可能无法读取空间 |

通过 `scripts/Check-Environment.ps1 -EngineRoot 'D:\sdk\UE_5.6'` 检查。
可设置当前PowerShell进程的 `CITYCOURIER_UE_ROOT`，或让脚本读取Epic安装清单。
脚本不会全局修改PATH或猜测引擎可用，也不会把Visual Studio存在当作UE编译成功。

本机新Launcher已显示安装完成，但旧版LauncherInstalled.dat仍为空；构建时明确传入上述EngineRoot。UE需要写用户目录下的构建缓存，受限沙盒中应申请正常构建权限。引擎包含核心、初学者内容、模板和源码；未安装MetaHuman核心数据、编辑器调试符号及额外目标平台。

登录由用户在Launcher界面完成，不需要提供账号密码。安装选择UE5.6正式版和Windows所需组件，组件与缓存占用需按Launcher实际显示的大小安排。

官方参考：[UE安装说明](https://dev.epicgames.com/documentation/unreal-engine/install-unreal-engine)、[UE5.6编译工具要求](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.6)。
