# CityCourier / 街头骑手

Windows + UE5.6，中国城市外卖员游戏。

**当前状态：开发环境与工程准备，尚不可玩。** 目前已创建原生UE工程、构建/测试/打包脚本和最小启动场景制作脚本；UE5.6尚未安装，未编译、未生成地图、未打包。后续驾驶、配送和城市美术仍按已批准计划开发。

开发目录：`D:\eleme\.worktrees\city-courier`，分支：`feat/city-courier`。

## 安装引擎后启动开发

在Epic Games Launcher中安装UE5.6正式版，然后在开发目录打开PowerShell。
如自动探测不到安装目录，给以下脚本传入 `-EngineRoot '实际引擎目录'`。

```powershell
.\scripts\Check-Environment.ps1
.\scripts\Build.ps1 -Target Editor
.\scripts\Run-EditorScript.ps1 -Script Content/Python/bootstrap_project.py
.\scripts\Test.ps1 -Filter CityCourier.Smoke -Headless
.\scripts\Package.ps1 -Configuration Development
```

这些是待执行的操作步骤，不是已经通过的验证记录。首次构建会生成缓存；生成的最小地图仅供确认引擎、光照、出生点和打包流程，完整街区在后续任务制作。

任务1打包成功后的预期入口：`Artifacts\Windows\Development\Windows\CityCourier.exe`。需要保留整个打包目录，不能只复制exe。当前不存在该交付物。

## 不依赖UE的工具验证

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/Tooling.Tests.ps1
```

此验证只检查构建工具的报告判断，不证明游戏功能正确。真实UE报告与日志输出在 `Saved\Validation`，不同运行使用独立目录以防旧报告误判。

## 项目文档

- [已确认设计](docs/superpowers/specs/2026-10-17-city-courier-design.md)
- [已确认实施计划](docs/superpowers/plans/2026-10-17-city-courier-implementation.md)
- [本机环境](docs/environment.md)
- [验证记录](docs/testing/acceptance.md)
