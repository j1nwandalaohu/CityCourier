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

所有新资产保留既有材质外观和来源记录。室内套件使用 `Buildings/Materials/` 下的独立副本，保存实例化渲染标记以支持打包；不修改原 `Art/Materials/` 资产。导入器会准备所需副本，已导入资产可运行 `prepare_enterable_materials.py` 单独修复绑定。结构墙、楼板和楼梯由编译后的厘米尺寸实例化，真实门洞不能被外观模型重新封住。门框和门扇分离；门远处代理与近处活动对象保持角度和阻挡一致。

## 重建流程

1. 使用 Blender 运行 `tools/art/build_enterable_assets.py`，生成新的 `.blend` 和 `Exports/Buildings/*.fbx`。
2. 先编译 Editor，再通过 `scripts/Run-EditorScript.ps1` 依次运行 `import_enterable_assets.py`、`export_building_inventory.py`、`build_building_catalog.py` 和 `build_enterable_city.py`。
3. 最后运行 `validate_enterable_city.py`，检查两张地图的完整编号集合、结构、每个房间的站立净空与地面，并用原生角色运动组件连续走过全部房间与门、返回街道。

所有脚本位于 `Content/Python/`。重建只在负责地图集成的功能工作树进行，不能覆盖另一个会话尚未提交的 `.umap`。源布局调整后必须重新编译目录并重建两张地图；不能只改一个副本。

全量物理行走采用加速固定步长调度，不能代替墙钟实机游玩或性能验收；两者分别记录在 [验证文档](../testing/enterable-buildings-validation.md)。


实际通行修正：所有外门向建筑外侧打开，避免堵住一层转弯；楼梯踏板厚 20 cm，禁止用楼面至踏步顶面的实心柱体，以免压低下一层回跑梯净空。家具放置检查真实导入边界、中央 120 cm 站立区和通路胶囊净空。

外门净空：`clear_enterable_entrances.py` 保留全部街道道具和碰撞，仅将门扇扫掠／接近通道内的盆栽、长椅、摊位移到旁边。原、新坐标及受影响 DoorId 记录在 `AssetsSource/CityCourier/Interiors/entrance-prop-placement.json`；不移动建筑或道路。地图重建自动执行，验证器独立检查全部 881 处外门。街道美术新增或改动道具后也必须通过该检查。
