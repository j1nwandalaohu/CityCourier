# Windows 试玩安装包

`scripts/Build-Installer.ps1 -EngineRoot D:/sdk/UE_5.6` 先生成 Shipping 包，再使用 NSIS 3.13 编译离线安装包。已有 Shipping 包时使用 `-SkipPackage`。产物位于 `Artifacts/Installers`，包括 Setup.exe、SHA256 和逐文件校验清单。

构建机需要 NSIS 3.13 官方便携版，解压到 `.dependencies/installer-tools/nsis-3.13`，或用 `-Compiler` 指定 makensis.exe。官方来源：https://nsis.sourceforge.io/Download 。第三方编译器不随游戏分发。

玩家只需要复制 Setup.exe。支持 Windows 10/11 x64，显卡需要 DirectX 12 / Shader Model 6，无需虚幻引擎。默认安装到当前用户的 `%LOCALAPPDATA%\Programs\CityCourierPlaytest`，创建桌面和开始菜单快捷方式，并在 Windows 已安装应用中登记卸载项。已有兼容运行库时不需要管理员权限；缺少运行库时单独请求系统确认来安装随包的 Visual C++ x64 运行库。

构建脚本优先使用本机 VS 提供的较新运行库，也支持 `-RuntimeRedistributable` 指定。PDB、Saved 目录和 ARM64 运行库不进入安装包。

安装器使用 Deflate 压缩以缩短试玩迭代的生成、安装时间，保持单个 EXE 离线安装。

卸载器核对安装标记，仅删除该版本清单中的文件与空目录，保留安装目录中玩家额外放入的文件，以及 `%LOCALAPPDATA%\CityCourier\Saved\SaveGames` 中的存档。

使用 `scripts/Test-Installer.ps1 -Installer <Setup.exe>` 验证安装、逐文件 SHA256、快捷方式、卸载登记、已安装 Shipping 版本启动及存档初始化、卸载清理和用户文件保留。测试安装目录限制在项目 `Saved/Validation` 下，不替换已有同名安装或快捷方式；启动检查使用独立 UserDir。

## 2026-10-19 试玩包验证

- `Artifacts/Installers/CityCourier-Setup-20261019-navigation.exe`，644,103,187 字节，包含屏幕立体导航箭头。
- 32 个安装文件的 SHA256 均与构建清单一致，当前用户快捷方式及卸载登记检查通过。
- 安装后的 Shipping 程序以独立 UserDir 和 RenderOffscreen 启动 30 秒，确认场景加载的内存占用和新存档初始化；该检查不模拟实体键鼠操作，最后终止本次测试的无窗口进程。
- 卸载移除了安装文件、快捷方式和登记项，保留额外用户文件，现有玩家存档未改变。
- 证据：`Saved/Validation/Installer-Smoke-20261019-184033-233a5ddf/installer-smoke.json`、`Saved/Validation/Interiors-Shipping-20261019-184121-d40d8321/shipping-startup.json`。

安装器 SHA256：`2293ce568f2616843320bc6a3517f76540035a575f593a1d337a17e6ecb97f6a`。

## 行人、车辆贴地修复版

后续试玩使用 `CityCourier-Setup-20261019-grounding.exe`（698,367,410 字节）；此前的 navigation 安装包不包含贴地修复。新包也保留屏幕立体导航箭头。

32 个安装文件的校验、安装后的 Shipping 无窗口启动检查、新测试存档初始化、卸载和原有玩家文件保留均通过。证据：`Saved/Validation/Installer-Smoke-20261019-214810-2cb8d159/installer-smoke.json`、`Saved/Validation/Interiors-Shipping-20261019-214819-908d8a06/shipping-startup.json`。

SHA256：`0752e5dee5f30e6331e5154227d82cc171e5ce073226bd1b51a0bf8648b111ec`。

## 引擎盖、车顶攀跳版

`CityCourier-Setup-20261019-car-roof.exe`（698,368,350 字节）包含两段汽车攀跳、随车转弯与跳下时的车速继承，同时保留此前的导航和贴地修复。

安装、32 个文件校验、安装后的 Shipping 启动、新测试存档初始化、卸载和现有存档保留均通过。证据：`Saved/Validation/Installer-Smoke-20261019-222815-726a898f/installer-smoke.json`、`Saved/Validation/Interiors-Shipping-20261019-222823-37a622f7/shipping-startup.json`。

SHA256：`3fb4dd7629491771a9a679638822efee586fad719908fc9fe869f1f569adbd3e`。
