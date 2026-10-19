# 室内装修与家具样板交付

后续全城接入已完成，范围、运行方式和正式地图验证见 [全城室内美术接入](city-interior-rollout.md)。下文保留首批三样板的独立交付记录。

本批以现有可进入建筑为基础，细化南方城市旧街区的商铺、住宅和办公室。全部资产位于独立目录 `/Game/CityCourier/Art/Interiors/`；不替换原建筑套件、角色、目录数据或地图。地图仍由功能工作树统一集成。

## 三个样板

| 用途 | 现有建筑编号 | 装修与陈设 |
| --- | --- | --- |
| 商铺 | `B_Merchant_0` | 地砖、旧灰泥、木质收边、玻璃金属门、取餐柜台、货架与包装用品 |
| 住宅 | `B_1_0_0_0_0` | 卧室床品、起居沙发、厨房橱柜、水槽、卫生间、公共走廊绿色墙裙 |
| 办公楼 | `B_0_1_0_0_0` | 桌椅、电脑、文件与储物架、办公室门、照明与走廊五金 |

这些编号、位置、朝向、楼层、房间、门、导航点和边均直接取自现有 `inventory.json` / `compile_building()`。`sample-layout.json` 是附加美术布置，不是新的建筑布局源。局部单位为厘米，Z 向上，建筑原点和朝向只应用一次。

## 独立资产

23 件网格包括七类家具、三种门扇、独立门框、踢脚线、公共区域墙裙、楼梯扶手立柱与横杆、空白房号底板、灯具、安全出口牌、开关、插座、踏步防滑收边，以及可供后续匹配真实窗洞的窗套和壁挂机。20 种独立材质涵盖灰泥、墙裙、地砖、木纹、布料、陶瓷、纸箱、金属、磨砂玻璃和灯罩。

灰泥使用仓库已保留的 Poly Haven CC0 扫描纹理。其余表面由项目原创材质图生成，带砖缝、粗糙度变化、细颗粒、木纹或织纹；几何由项目脚本生成。出口字样使用已有 Noto Sans CJK 字体（OFL）。本批不引入付费素材或额外软件下载。来源记录仍保留在 `AssetsSource/CityCourier/External/`；新网格、材质清单与校验报告位于 `AssetsSource/CityCourier/InteriorArt/`。

家具以底部原点交付，朝向局部 -Y。组合按房间真实尺寸选择：主家具必需，次家具只有在不重叠、避开开启扇区和全部路线时才放入。禁止以关闭家具或扶手碰撞来通过测试。窗套与空调作为独立库存交付，不自动在无洞外墙上新增假窗。

## 门和功能接口

- 标准门扇净宽 100 cm、高 220 cm、主体厚约 5 cm；原点在左下铰链，宽沿 +X，绕 Z 打开。把手、锁孔、合页和踢脚护板随门扇移动。
- `SM_IF_DoorFrame` 独立于门扇，U 形门洞保留净宽与净高。每个门的尺寸适配使用原有 `width/100`、`height/220`；门框和门扇使用相同铰链位置和关闭角。
- `door_styles` 只描述按 DoorId 替换现有移动门扇的网格。此交付不生成静止装饰门堵住路线，也不修改原有 `CourierDoor` 开门逻辑和独立叶片碰撞。
- 房号底板为空白；文字仍由功能系统生成。不会在美术脚本里复制或重新编号。
- 扶手及底座放在现有梯段之间，保留原 120 cm 踏步宽。踢脚线使用现有墙体片段，不横跨门洞。

样板脚本只在临时未保存关卡中覆盖新建筑 Actor 的组件材质：`M_Plaster → M_IF_Plaster`、`M_Concrete → M_IF_TileGrey`、`M_PaintedMetal → M_IF_MetalDark`、`M_Wood → M_IF_WoodLight`。灰色地砖材质在朝下表面显示浅色天花板；没有编辑共享立方体或原材质包。

## 重建与验证

在本工作树根目录运行：

```powershell
& D:/eleme/.dependencies/art-tools/blender-4.5.3-windows-x64/blender.exe --background --python-exit-code 1 --python tools/art/build_interior_finish.py
& D:/sdk/Python/Python314/python.exe -m tools.interiors.finish_kit
$env:CITYCOURIER_INTERIOR_IMPORT_SCOPE='all'
$interiorProject=(Resolve-Path ./CityCourier.uproject).Path
$interiorImporter=(Resolve-Path ./Content/Python/import_interior_finish.py).Path
& D:/sdk/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe $interiorProject -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput -AllowCommandletRendering -run=pythonscript "-script=$interiorImporter"
./scripts/Run-EditorScript.ps1 -Script Content/Python/validate_interior_finish.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/capture_interior_finish.py -EngineRoot D:/sdk/UE_5.6 -Gui
```

UE 进程串行运行。导入器仅保存本套件目录；场景、通行检查和截图脚本均不保存任何 `.umap`。导入报告检查材质槽、真实导入边界与受保护文件哈希。布局测试检查 120 cm 站立区、34 cm 胶囊另加 11 cm 余量、全部路线、门扇完整扇区及家具间重叠。

首次生成材质需要 `-AllowCommandletRendering`，否则无渲染器的命令行模式无法取得实际着色器统计。已有绿色材质报告、且 `palette.json` 未变化时，可设 `$env:CITYCOURIER_INTERIOR_IMPORT_SCOPE='meshes'`，再通过 `Run-EditorScript.ps1` 只重导网格；导入器会核对已编译材质及 palette 哈希，不接受空的导入结果来替代重导。

本次实际导入 23 件网格、20 种已编译材质，网格边界与源文件最大差异为 0.00000960 cm。FBX 导出阶段补偿旧版导入器的 Y 反射，同时保留三角形绕序与角点法线；Blender 可编辑模型仍使用上述标准坐标。门框还检查实际面片，保证收边不伸入 100×220 cm 净洞。

`physical-validation.json` 的实际结果：

| 样板 | 附加美术件 | 房间走访 | 实际开门 | 路程 | 返回街道 |
| --- | ---: | ---: | ---: | ---: | --- |
| 商铺 | 562 | 43/43 | 35/35 | 389.02 m | 是 |
| 住宅 | 674 | 51/51 | 44/44 | 486.82 m | 是 |
| 办公楼 | 691 | 51/51 | 45/45 | 554.81 m | 是 |

补齐半层平台后墙灯后，最终实际通行重新通过，记录为 `EditorScript-20261019-093020-2af5abb1`。所有模型边界、碰撞配置和房间检查均无错误，受保护资源在检查前后哈希一致。纯 Python 测试 32/32 通过；已有免费来源 116 文件的 SHA-256 校验通过。

照明使用模型灯具下的实际矩形光源，商铺 4500 K、住宅 3800 K、办公室 5000 K。半层平台的后墙灯安装在平台以上 225 cm，灯罩和光源均朝楼梯方向；不把灯放在楼梯通行头顶。截图采用固定物理曝光，完整参数和临时渲染设置写入截图报告。地图集成方可直接采用这组灯具位置与参数，再单独进行全城性能验收。

最终八张实际 UE 视口图见 [可离线打开的样板相册](interior-samples/index.html)，运行 `20261019T015707Z` / `EditorScript-20261019-095656-222e6455`，进程退出 0。每张 1600×1000，完整墙顶和门扇保留；相机与实际视口的位置偏差均为 0 cm。PNG 按原字节交付，未后期修图，校验见 [images.json](interior-samples/images.json)，原始参数见 [capture-results.json](interior-samples/capture-results.json)。图片用于当前样板评审，不宣称已经达到照片级或完成全城游戏内验收。

后台编辑器若处于非实时模式，UE 会把 TSR 回退为 FXAA，导致软阴影明显噪点。截图脚本通过原生 `Stats Display` 请求实时视口，并仅在当前进程关闭后台节流；高分辨率截图不绘制版本文字。设置类使用原生反射读取，不写入用户配置。退出采用编辑器 Python 执行器的延迟退出机制，避免重复退出。

物理检查使用现有原生角色运动、真实门碰撞、楼梯、所有目标房间和返回街道的路径，保留新家具及扶手的阻挡碰撞。它采用加速固定步长，不能代替墙钟实机性能验收。真实 UE 截图单独记录，完整墙体、楼板和天花板保留；截图中的门扇展示姿态不等于新增运行时门逻辑。报告明确区分源码几何、实际导入、物理通行与画面检查。

本批只验证三个样板。全城批量应用与最终游戏内灯光由地图集成方在样板验收后进行；不得把样板结果写成全城验证。七段补充人物动画见 [pending-actions.md](pending-actions.md)。

集成时采用新布局中的附加家具与灯具位置，不要把新组合叠在原生 `PrepareDetails()` 生成的旧家具上。验证脚本刻意不准备旧家具，真实门和结构仍由原生系统承担；功能方需要在自己的详情生成流程中替换对应家具组。截图脚本使用独立 Actor 便于逐件检查；全城应沿用现有实例化策略按网格分组，避免直接复制样板的逐件 Actor 数量。

为避免补齐动画后出现停跑跳脚，本分支另有一处很小的动画联调：仅 `A_RunStop` 的混入时间从 30 ms 调整到 150 ms，其他混合参数、动作时长、角色移动和建筑功能不变。该修改与室内地图集成无关，验证与证据单独记录在人物动画交付文档中。
