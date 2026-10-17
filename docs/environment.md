# 本机开发环境

检查日期：2026-10-17。

| 项目 | 观察结果 |
| --- | --- |
| GPU | NVIDIA GeForce RTX 3060 Ti，8192 MiB显存 |
| 内存 | 约15.8 GiB |
| Visual Studio Build Tools | 2022 / 17.14.29 |
| 已有MSVC | 14.44.35207；须由UE实际构建确认兼容性 |
| 已有Windows SDK | 10.0.22621.0 |
| Epic Games Launcher | 已通过winget安装，安装程序哈希校验通过 |
| Launcher可执行文件 | `C:\Program Files\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe` |
| UE5.6 | 尚未检测到，等待用户登录Launcher并安装 |
| Blender | 发现 `D:\tools\Blender Foundation\Blender 4.0` 残留目录，未发现blender.exe，不能视为可用 |
| 检查时空闲磁盘 | C约71.5 GiB，D约55.4 GiB；以安装时实际空间为准 |

引擎安装后，通过 `scripts/Check-Environment.ps1 -EngineRoot '实际目录'` 检查。
可设置当前PowerShell进程的 `CITYCOURIER_UE_ROOT`，或让脚本读取Epic安装清单。
脚本不会全局修改PATH或猜测引擎可用，也不会把Visual Studio存在当作UE编译成功。

登录由用户在Launcher界面完成，不需要提供账号密码。安装选择UE5.6正式版和Windows所需组件，组件与缓存占用需按Launcher实际显示的大小安排。

官方参考：[UE安装说明](https://dev.epicgames.com/documentation/unreal-engine/install-unreal-engine)、[UE5.6编译工具要求](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.6)。
