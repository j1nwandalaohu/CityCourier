# 房间与商户写实美术升级

本次保留现有建筑、房间、门洞、家具外部尺寸和通行布局，升级室内公共材质及六套商户陈设。不是完整扫描店铺，也不声称达到照片级效果。

## 已接入的素材来源

原始下载文件、提供方元数据、许可页面快照和逐文件 SHA256 见 `AssetsSource/CityCourier/External/RealisticInteriors/sources.json`。下载器核验 Poly Haven 提供的 MD5；导入前再次验证本地 SHA256，缺文件、改动、未知许可或缺失 CC BY 署名均拒绝导入。

| 原始资产 | 用途 | 许可 |
| --- | --- | --- |
| [Poly Haven Food Apple 01](https://polyhaven.com/a/food_apple_01) | 便利店收银台水果托盘，保留扫描 UV 与 1K PBR 贴图 | CC0-1.0 |
| [Poly Haven Food Ginger 01](https://polyhaven.com/a/food_ginger_01) | 面馆备餐区生姜，保留扫描 UV 与 1K PBR 贴图 | CC0-1.0 |
| [Poly Haven Wooden Cutting Board](https://polyhaven.com/a/wooden_cutting_board) | 面馆和烧腊柜台的木砧板，保留原始 UV | CC0-1.0 |
| [Poly Haven Tiled Floor 001](https://polyhaven.com/a/tiled_floor_001) | 房间地砖，2K 颜色、法线、粗糙度；覆盖 150 cm 实际尺寸 | CC0-1.0 |
| [Poly Haven Wood Table 001](https://polyhaven.com/a/wood_table_001) | 木制家具、门框和木饰面，2K PBR | CC0-1.0 |
| [ambientCG Metal 009](https://ambientcg.com/a/Metal009) | 操作台、锅具及金属陈设，2K 颜色、法线、粗糙度及金属度 | CC0-1.0 |
| 项目已有 Poly Haven Painted Plaster Wall | 墙面保留实际漆面纹理，不再仅叠加 35% 扫描表面 | CC0-1.0 |

许可原文：[Poly Haven](https://polyhaven.com/license)、[ambientCG](https://docs.ambientcg.com/license/)。这些来源允许将原文件随公开游戏工程分发。

`Whole Roasted Chicken`（inciprocal，CC BY 4.0）已核实页面许可，但官方文件下载接口要求用户登录，**未下载、未打包，也未用它冒充扫描烧鸭**。中式挂烧鸭未找到已确认可直接使用的开放文件，因此按用户认可的备选方案重新建模：胸腹、肩部、曲颈、头部、折叠翅膀及腿部；不同体型、角度的三只鸭替代七个简化轮廓。使用项目原创烤皮材质，增加烤色变化、油光及粗糙度变化。该模型不是扫描资产。

同时补充原创建模的碗、酱油瓶、柜门与把手。新物件均位于原家具内部包围盒中；六个商户整体导出包围盒与升级前完全一致，保留原生阻挡碰撞。扫描小物件每个网格约束到 3,500 三角面以内，UV 随网格保留；六套陈设均低于既有 60,000 三角面限制。没有新增灯光或调整曝光来掩盖材质问题。

## 重建和验证

```powershell
D:/sdk/Python/Python314/python.exe tools/art/fetch_realistic_interiors.py
.dependencies/art-tools/blender-4.5.3-windows-x64/blender.exe --background --python-exit-code 1 --python tools/art/build_shop_furnishings.py
# 首次更新材质需要真实 RHI，见 import_interior_finish.py 顶部命令说明。
# 材质已编译且 palette.json 一致时，后续网格更新可复用已验证材质。
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_shop_furnishings.py -EngineRoot D:/sdk/UE_5.6
D:/sdk/Python/Python314/python.exe -m tools.interiors.validate_shop_furnishings
./scripts/Run-EditorScript.ps1 -Script Content/Python/capture_realistic_interiors.py -Gui -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/validate_city_finish.py -EngineRoot D:/sdk/UE_5.6
```

正式地图实拍保存在 `Saved/Validation/RealisticInteriors/Captures`。脚本检查实际原生家具已驻留并保留阻挡碰撞，使用默认游戏曝光，覆盖三个临街商户、食品近景和楼上房间。最终以实际截图及原生通行报告为验收证据；本次不把截图当成性能测试。

本次验证：40 项 Python 检查通过；29 个网格与 25 个材质导入通过，包围盒误差小于 0.0001 cm。8 个正式地图机位完成实拍，烧鸭曲面与材质调整后另行复核了近景及街景（`EditorScript-20261019-142937-b1d8946a`）。正式地图与预览地图各 293 栋建筑、18,321 个房间、15,697 扇门的原生角色全量通行通过（`EditorScript-20261019-143100-08d243be`）。报告为 `Saved/Validation/city-finish-runtime.json`。食品 glTF 的灰度粗糙度纹理不会误作环境遮蔽；只有提供方真正打包的 ARM 图才从 R 通道读取 AO。

最终 Windows Development 打包通过（`Package-Development-20261019-143345-5525e914`，`BUILD SUCCESSFUL`、退出码 0），输出为 `Artifacts/Windows/Development/CityCourier.exe`。
