# 剩余街景与室内套件接入

正式地图 `L_CourierCity` 从美术预览地图同步街景布置：写实树木、旧空调、木箱、三种摊位、排水沟盖、井盖、轮式垃圾桶及配套灯光。道路、人行道、地面改用现有扫描材质，并接入傍晚全景天空与对应天空光。关闭新增后期中的胶片颗粒，避免恢复已有闪烁问题。

街景沿用预览地图的视觉装饰策略，不新增路线碰撞；原有街道碰撞、293 栋可进入建筑和配送对象保留，门口物理净空仍需通过检查。天气和天空光只应用现有美术设定，没有新增天气玩法。

室内剩余三种模型纳入原生按距离加载的 `DA_InteriorFinish`：

- 防滑收边贴合真实梯段踏步，厚度不足 1 cm，保持原踏步碰撞；位置和宽度必须与指定踏步一致。
- 壁挂空调按房间高处外侧墙面布置，避免门扇扫掠和路线。
- 窗套匹配现有立面封闭玻璃窗的位置和尺寸；不新增可穿行窗洞，也不改变房间、楼层、导航或建筑结构。

全城补充后共 284,946 个美术实例，其中踏步收边 30,952 条、壁挂空调 5,221 台、外窗饰面 3,504 个。模型在原有详情准备／卸载路径中分组实例化。

正式运行启用与美术预览一致的虚拟阴影贴图。传统阴影在密集室内灯光场景中触发了 D3D12 的 1,024 个并发 residency command-list 上限，错误被引擎报告为显存不足；出错时显存使用约 3.1 GB，未达到 7.2 GB 预算。启用虚拟阴影后完整六楼路线可运行。中档阴影页池设为 2,048 页，避免引擎原 512 页池溢出产生缺失阴影。打包验收脚本现在把页池溢出也视为渲染失败。

旧版动作、旧家具、角色对比试验和备份模型继续保留，不能把这些历史资源当作正式美术遗漏后重复叠放。

重建步骤（UE 进程串行执行）：

```powershell
& D:/sdk/Python/Python314/python.exe -m tools.interiors.finish_kit
& D:/sdk/Python/Python314/python.exe -m tools.interiors.build_city_finish
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_city_finish.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/integrate_environment_art.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/validate_environment_integration.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/validate_city_finish.py -EngineRoot D:/sdk/UE_5.6
./scripts/Test.ps1 -Headless -EngineRoot D:/sdk/UE_5.6
./scripts/Package.ps1 -EngineRoot D:/sdk/UE_5.6
```

引用检查同时要求：9 类街景模型实际可见，灰盒道路／人行道／地面材质不再用于这些表面，以及室内套件全部 23 种模型均由运行时数据资产引用。几何检查、实机图片和打包帧率需分别核对，不能相互代替。

已完成的几何与回归记录：Python 34 项、UE 93 项通过；两张地图各 293 栋／18,321 房间／15,697 扇门全量通行通过。街景接入重复执行通过，306 个原生建筑与配送对象的变换数值不变。原生室内八张视口截图运行 `EditorScript-20261019-105532-cda0dfb2` 完成且退出 0，受保护资产哈希未变。截图只作视口画面证据，性能以单独打包版测量为准。

打包版检查：车辆上下车／禁止骑入建筑通过，人物动作检查缺失资源为 0；动作报告仍不代表全部动作视觉验收。虚拟阴影与 2,048 页池的三分钟路线完成六楼到达、卸载重进和门状态恢复，未再出现崩溃或阴影页池溢出。i5-13400F／RTX 3060 Ti／16 GB、1080p 中档、83% 内部分辨率、软件 Lumen GI／反射下，平均 **56.91 FPS**，1% low **18.93 FPS**：**未达到 24 FPS 的低帧目标，严格性能预检仍失败**。这项未通过结果保留在 `Saved/Validation/Interiors-Acceptance-20261019-111557-377ecf30/Performance/`；不能把资源接入完成写成性能验收通过。

最终街景与夜市的实际 PIE 图片位于 `Saved/Validation/CompleteArt-Exterior-Final/`，已核对新旧摊位、路灯及旧悬空装饰招牌不再叠放。之后的灯光单位同步重新保存了地图；上述性能值是同一渲染配置下代表路线的实测记录，最终地图的严格性能验收尚未通过。
