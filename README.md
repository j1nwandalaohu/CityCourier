# CityCourier / 街头骑手

Windows + UE5.6，中国城市外卖员游戏。

**当前分支：独立美术预览。** 基于任务8的玩法版本，加入中国南方城市街区、傍晚光照、骑手与电动车美术。默认打开 `L_CourierArtPreview`；原玩法地图 `L_CourierCity` 保留。此分支与另一 Codex 的后续玩法开发分开，尚未合并。

美术目录：`D:\eleme\.worktrees\city-courier-art`，分支：`feat/city-courier-art`。

直接打开本目录的 `CityCourier.uproject`，点击 Play 即可预览。美术预览图在 `Saved/Validation/Art-PIE`，Blender 源文件在 `AssetsSource/CityCourier/Blender`。本分支没有生成新的发行包；旧的 Artifacts 不能代表当前效果。

已保存的实际游戏画面：[街景](docs/art/previews/street.png)、[骑手](docs/art/previews/rider.png)、[骑乘](docs/art/previews/riding.png)、[摊位街](docs/art/previews/market.png)。

外部素材仅使用免费 CC0 人体、服装、皮肤、PBR 材质和环境光；中文字体为 SIL OFL。来源详见 [素材清单](docs/art/asset-manifest.csv)，原文件及授权记录在 `AssetsSource/CityCourier/External`。建筑、电动车、街景道具与动画由项目自行制作。

重建美术（先安装 Blender 4.5 LTS；已提交导入后的 UE 资源，普通预览不需要重建）：

```powershell
python tools/art/fetch_sources.py
python tools/art/fetch_human_pack.py
blender --background --factory-startup --python tools/art/build_courier_assets.py
.\scripts\Build.ps1 -Target Editor
.\scripts\Run-EditorScript.ps1 -Script Content/Python/import_art.py
.\scripts\Run-EditorScript.ps1 -Script Content/Python/build_art_city.py
.\scripts\Run-EditorScript.ps1 -Script Content/Python/capture_art.py -Gui
```

合并时应先合入美术源代码和资源，再对目标玩法分支运行 `import_art.py`。若目标城市布局有变化，应在目标分支重新生成美术预览地图；现有美术地图是基于提交 `2395e0d` 的快照。不要覆盖另一条分支正在编辑的 Blueprint 或地图文件。

## 构建与测试

在开发目录打开PowerShell；本机引擎路径为 `D:\sdk\UE_5.6`。
如自动探测不到安装目录，给以下脚本传入 `-EngineRoot '实际引擎目录'`。

```powershell
$env:CITYCOURIER_UE_ROOT = 'D:\sdk\UE_5.6'
.\scripts\Check-Environment.ps1
.\scripts\Build.ps1 -Target Editor
.\scripts\Test.ps1 -Filter CityCourier -Headless
```

实际验证结果见 [验证记录](docs/testing/acceptance.md)。首次构建会生成缓存。当前美术为可玩的首版，尚非照片级效果；建筑重复度、手指握把、鞋履、表情和更细致的布料变形仍需打磨，未进行最终发行性能验收。

历史任务1打包入口为 `Artifacts\Windows\Development\CityCourier.exe`，不包含当前玩法和美术；本次没有重新生成发行包。

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
