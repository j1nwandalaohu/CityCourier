# 一层真实商铺的专属陈设

可进入建筑转换时，旧店景与实心核心一起被过滤；通用 Counter/Shelf 没有保留面馆、便利店和烧腊店的区别。本次将 `storefront_detail.py` 的原有陈设提取为独立房间组件，接入原生 `DA_InteriorFinish`，不恢复旧实心外壳或展示店景。

一层 `shop` 房间按楼体原有 A/B/C 变体选择：

| 变体 | 必需主陈设 | 净空允许时添加 |
| --- | --- | --- |
| A 面馆 | 汤锅、操作台、排烟罩、菜单 | 原有餐桌、凳子和餐具 |
| B 便利店 | 收银台、收银机、货箱 | 重新排布的冷饮柜与有货货架 |
| C 烧腊店 | 展示架、挂钩、烧腊、砧板及操作台 | 原有餐桌和凳子 |

沿街楼两侧一层商铺及四个取餐商户共 **580 间**均有对应主陈设；当前共 1,160 组商铺陈设。主陈设正面朝街，优先沿后墙摆放；狭小取餐店将柜台深度适配为约 40 cm，同时保留完整中央站立区。标准主陈设约 150×45 cm，餐桌组约 105×65 cm，物件高度保留。便利店冷柜和货架单独调整横向尺寸并重新并排放置，避免将整排店景压缩成微型电器。

家具仍参与真实碰撞；布局检查真实导出边界、房间包围盒、120 cm 中央站立区、全图通路胶囊、完整门扇扫掠及家具间重叠。上层外壳、顶灯、招牌和雨棚继续使用已有资产，取餐绑定保持不变。楼梯与相关导航调整见 `wide-stairs.md`。

朝街墙体必须有真实橱窗开口，不能只把陈设放到封闭房间中。一层商铺前墙分解为窗台、过梁和立柱，橱窗使用 `Buildings/Materials/M_ShopGlass` 通透玻璃（透明度输入 0.06），保持阻挡碰撞。玻璃组件不投射阴影、不参与距离场遮挡，并禁用 Nanite，避免不支持透明材质时退回默认不透明表面。中央建筑入口及原房间门洞保留，玻璃不作为可穿行的假入口。不新增补光，也不改变室内外亮度设置。

重建（在主目录按顺序执行）：

```powershell
.dependencies/art-tools/blender-4.5.3-windows-x64/blender.exe --background --python-exit-code 1 --python tools/art/build_shop_furnishings.py
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_shop_furnishings.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/prepare_shopfront_glass.py -EngineRoot D:/sdk/UE_5.6
D:/sdk/Python/Python314/python.exe -m tools.interiors.finish_kit
D:/sdk/Python/Python314/python.exe -m tools.interiors.build_city_finish
D:/sdk/Python/Python314/python.exe -m tools.interiors.validate_shop_furnishings
./scripts/Run-EditorScript.ps1 -Script Content/Python/build_building_catalog.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_city_finish.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/build_enterable_city.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/validate_city_finish.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/capture_street_shops.py -Gui -EngineRoot D:/sdk/UE_5.6
```

完整 `build_interior_finish.py` 也包含六套店铺组件；增量模型导入只修改这些组件，保留原 23 个模型及材质。墙体与橱窗通过建筑目录和两张地图重建更新。检查报告为 `shop-import-validation.json`、`shop-plan-validation.json`。37 项 Python 回归通过，六个模型的导入边界误差均小于 0.0001 cm。新增组件的文字镜像已在导出适配中修复，Blender 可编辑源保持可读。

临街验收使用 `capture_street_shops.py`，相机位于主街，使用实际 PIE、正式地图、原生陈设与默认游戏曝光；同时检查真实玻璃组件透明模式及碰撞。三个机位均完成且无脚本错误（`EditorScript-20261019-132249-9b2528a1`），图片为 `Saved/Validation/StreetShops/after/B_0_0_0_0_0.png`、`B_0_0_0_0_1.png`、`B_0_0_0_0_2.png`。已目视确认面馆操作台、便利店货架、烧腊展示柜从街道可见。旧 `capture_shop_furnishings.py` 仅作室内参考，不能替代临街验收。

四个附属取餐楼位于较窄的内部通道，不能把相机简单放在它们正前方 6 m 当作主街机位；该位置可能已进入另一栋建筑。脚本选择实际朝向主街的三种沿街楼。取餐楼的玻璃和陈设同样纳入全城布局检查，原生通行单独验证。截图不是墙钟性能测试，既有低帧指标尚未重新认证。

最终回归：93 项 UE 自动化通过（`Automation-20261019-132725-35ab593c`），正式地图和预览地图各 293 栋／18,321 房间／15,697 扇门的原生全量通行通过（`EditorScript-20261019-132519-f6da005b`）。玻璃保留阻挡碰撞，未通过关闭碰撞来伪造临街开口。
