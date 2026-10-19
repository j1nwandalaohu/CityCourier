# 全城楼梯与转角平台加宽

292 栋多层建筑的单跑楼梯由 120 cm 加宽到 **160 cm**，半层转角平台由 140 cm 加深到 **180 cm**。30,952 级踏步和 1,460 处平台统一更新；28 cm 踏步深度、20 cm 踏板厚度、原有楼层标高和抬腿高度不变。单层驿站不新增楼梯。

楼梯井改为 366×680 cm，压缩原来中央隔墙两侧的空隙；扶手底座按真实模型边界放入 26 cm 中央间隙，保留完整踏步宽度。建筑外轮廓、世界位置、旋转、楼栋/房间/门 ID 和配送绑定保持原有身份。相邻储物间边界和相关门洞随楼梯井调整。

通往后侧储物间的路线增加门内节点，先正对门洞通过，再转向房间站立点，避免原长斜线切过卧室家具角。家具优先保留 6 cm 离墙间距；放不下时尝试 2 cm 离墙间距，并完整保留中央站立区、碰撞、门扇扫掠及通路胶囊检查。可选的第二件家具仍以净空为准。

重建按序执行，不能只改楼梯模型而保留旧碰撞或旧导航：

```powershell
D:/sdk/Python/Python314/python.exe -m tools.interiors.finish_kit
D:/sdk/Python/Python314/python.exe -m tools.interiors.build_city_finish
./scripts/Run-EditorScript.ps1 -Script Content/Python/build_building_catalog.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_city_finish.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/build_enterable_city.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/validate_city_finish.py -EngineRoot D:/sdk/UE_5.6
```

`build_enterable_city.py` 的保护快照改为数值变换，避免把 Python 临时结构包装地址误认为物体移动。两张地图同步重建，原有灯光和商户专属陈设继续保留。

对比脚本 `capture_wide_stairs.py` 从实际 UE 建筑数据读取尺寸，使用相同游戏视角和曝光拍摄梯段、转角。通过环境变量 `CC_STAIR_STAGE=before` 保存旧图；默认拍摄更新后的数据，输出到 `Saved/Validation/WideStairs/`。截图中角色临时定位用于详情驻留，不作为真实行走证明；通行由原生角色独立验证。既有性能低帧目标尚未通过，本次不将布局改善视为性能验收通过。

验证记录：36 项 Python 测试通过；完整 UE 自动化 93 项通过（`Automation-20261019-122318-e11561f5`）。正式地图和预览地图同步重建后，各自 293 栋、18,321 房间、15,697 扇门的原生角色全量通行均通过（`EditorScript-20261019-122128-65529d69`）。580 间一层商铺的专属陈设与当前几何净空复查通过。旧版实拍数据为 120 cm 梯段、140 cm 平台。

更新后 PIE 两个机位完成且无脚本错误（`EditorScript-20261019-122408-e18d8624`），原生目录实测梯段 160 cm、平台深 180 cm、平台宽 366 cm。前后视角和游戏曝光一致，机位相对当前梯段和平台定位。

Development 打包 `Package-Development-20261019-122542-9bb5c9da` 成功退出 0，更新后的主地图游戏随后启动。
