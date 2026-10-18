# 可进入建筑资产与布局接口

本功能使用独立的 `/Game/CityCourier/Art/Buildings/` 目录。原人物、动画、建筑外观和材质资产保留；新楼体基于既有 `build_environment.py` 重建外部细节，移除实心核心、浅店景及假门。完整几何清单见 `AssetsSource/CityCourier/Interiors/asset-manifest.json`。

## 共用数据

- `AssetsSource/CityCourier/Interiors/inventory.json` 是清点后的来源、稳定编号、实际落位和地点绑定；包含 288 个沿街模块、四处附属商家和单层驿站。
- 门楼为入口构筑物，关联真实楼栋地址，不虚增居住楼层。附属商家原展示位置与邻楼重叠时在同街区调整，清单同时保存原坐标和调整原因；驿站按街角可用空间适配宽度。
- `tools/interiors/layout.py` 是房间、楼层、门洞、逐级楼梯和通行节点的唯一布局来源。紧贴的楼栋之间不生成侧门；这些不可见党墙位置没有装饰性假门，房间仍从本楼公共入口与楼梯到达。
- 局部坐标为厘米、Z 向上、前入口朝 -Y；楼体旋转只应用一次。外观沿用 Blender 米制源文件和既有导入约定，由楼体 Actor 加固定前向转换。

## 资产约定

| 资产 | 用途与原点 |
| --- | --- |
| `SM_EnterableShopHouseA/B/C` | 与原楼层一致的空心细节外壳；世界原点在楼体底部中心 |
| `SM_EnterableAnnex` | 单层驿站前脸；原始宽 650 cm、深 500 cm，运行落位按数据适配 |
| `SM_CourierDoor` | 100×5×220 cm 标准门；原点为左下铰链，门宽沿 +X，绕 Z 打开 |
| `SM_InteriorBed` | 单人床与床品，底部中心原点 |
| `SM_InteriorDesk` | 办公桌和显示器，底部中心原点 |
| `SM_InteriorKitchen` | 厨房柜台、水槽、灶台，底部中心原点 |
| `SM_InteriorToilet` | 卫生间器具，底部中心原点 |
| `SM_InteriorShelf` | 储物架与货物，底部中心原点 |
| `SM_InteriorSofa` | 起居区座椅，底部中心原点 |
| `SM_InteriorCounter` | 服务／取餐柜台与餐盒，底部中心原点 |

所有新资产复用项目既有材质和来源记录。结构墙、楼板和楼梯由编译后的厘米尺寸实例化，真实门洞不能被外观模型重新封住。门框和门扇分离；门远处代理与近处活动对象保持角度和阻挡一致。

## 重建流程

1. 使用 Blender 运行 `tools/art/build_enterable_assets.py`，生成新的 `.blend` 和 `Exports/Buildings/*.fbx`。
2. 先编译 Editor，再通过 `scripts/Run-EditorScript.ps1` 依次运行 `import_enterable_assets.py`、`export_building_inventory.py`、`build_building_catalog.py` 和 `build_enterable_city.py`。
3. 最后运行 `validate_enterable_city.py`，检查两张地图的完整编号集合、结构和每个房间的站立净空与地面。

所有脚本位于 `Content/Python/`。重建只在负责地图集成的功能工作树进行，不能覆盖另一个会话尚未提交的 `.umap`。源布局调整后必须重新编译目录并重建两张地图；不能只改一个副本。

结构覆盖报告仅证明清单、碰撞和站立位置，不能代替完整行走、门交互、正式陈设、配送或性能验收。后续运行结果记录在测试文档中。
