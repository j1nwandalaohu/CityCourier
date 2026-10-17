# CityCourier / 街头骑手

Windows + UE5.6，中国城市外卖员游戏。

**当前状态：最小工程验证完成，配送玩法尚未实现。** UE5.6.1编辑器编译、启动地图测试、Windows Development打包和实际画面/移动输入验证已通过；目前是地面和天空组成的验证场景。后续驾驶、配送和城市美术按已批准计划开发。

开发目录：`D:\eleme\.worktrees\city-courier`，分支：`feat/city-courier`。

## 安装引擎后启动开发

在开发目录打开PowerShell；本机引擎路径为 `D:\sdk\UE_5.6`。
如自动探测不到安装目录，给以下脚本传入 `-EngineRoot '实际引擎目录'`。

```powershell
$env:CITYCOURIER_UE_ROOT = 'D:\sdk\UE_5.6'
.\scripts\Check-Environment.ps1
.\scripts\Build.ps1 -Target Editor
.\scripts\Run-EditorScript.ps1 -Script Content/Python/bootstrap_project.py
.\scripts\Test.ps1 -Filter CityCourier.Smoke -Headless
.\scripts\Package.ps1 -Configuration Development
```

实际验证结果见验证记录。首次构建会生成缓存；生成的最小地图仅供确认引擎、光照、出生点和打包流程，完整街区在后续任务制作。

最小验证场景的打包入口：`Artifacts\Windows\Development\CityCourier.exe`。需要保留整个打包目录，不能只复制exe；此版本尚无配送玩法。

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
