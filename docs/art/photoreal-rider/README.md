# 正式骑手人物细节与 UE 实拍

本轮直接升级 `/Game/CityCourier/Art/Characters/SK_Courier`。身体、服装和动作继续使用正式骑手，头部采用已保留的 CC0 CharMorph-Vitruvian 男性、EastAsian 形态并拟合到原骨架。没有付费素材，也没有用生成图片代替游戏人物。

原版在 `Art/Portrait/Baseline/SK_CourierBefore` 保留。对比使用相同姿势、相机和光源，1800×1200 像素，固定曝光，关闭景深、运动模糊、颗粒、晕影和泛光。所有图片来自实际 UE PIE 渲染；自动化通过只证明资源、姿势与取景约束，不能替代照片感的人工判断。

当前达到高细节写实游戏人物的水平，发丝边缘和眼神仍有 CG 感；未宣称已经与真人照片不可区分。

## 实拍对比

| 场景 | 原版 | 本轮正式人物 |
| --- | --- | --- |
| 中性光、无头盔 | [查看](before/head-bare-neutral.png) | [查看](after/head-bare-neutral.png) |
| 傍晚光、无头盔 | [查看](before/head-bare-dusk.png) | [查看](after/head-bare-dusk.png) |
| 中性光、戴头盔 | [查看](before/head-helmet-neutral.png) | [查看](after/head-helmet-neutral.png) |
| 侧脸与发际线 | [查看](before/profile-bare-neutral.png) | [查看](after/profile-bare-neutral.png) |
| 奔跑动作取样 | [查看](before/fullbody-run-sample.png) | [查看](after/fullbody-run-sample.png) |
| 骑姿动作取样 | [查看](before/fullbody-ride-sample.png) | [查看](after/fullbody-ride-sample.png) |
| 原生移动中的人物 | [查看](before/native-gameplay-run.png) | [查看](after/native-gameplay-run.png) |
| 实际上车后的骑手 | [查看](before/native-gameplay-ride.png) | [查看](after/native-gameplay-ride.png) |

![本轮正式骑手，中性光实拍](after/head-bare-neutral.png)

## 已接入的细节

- 4K 头部肤色、区域粗糙度和由上游高度图生成的切线法线；身体保留原贴图，独立 UV1 与顶点蒙版处理颈部过渡。
- Subsurface Profile 皮肤、细孔和细纹；肤色按线性空间混合，法线与粗糙度按线性数据导入。
- 独立虹膜、瞳孔、巩膜、角膜及眼睑湿线。透明眼表保留反光，关闭会把屏幕图像扭入眼球的折射偏移。
- 1.6 万根短发、眉毛、144 根上下睫毛与稀疏胡茬使用实际蒙皮几何；发丝使用 UE Hair 着色。耳周以椭圆羽化，避免矩形剃缺。
- 原身体与服装的顶点位置和权重保留，头部和颈部绑定原有骨骼，颈部重叠区转移原身体权重。

## 复现

在本 worktree 根目录执行，避免同时有编辑器写入资产：

```powershell
$blender = 'D:\eleme\.dependencies\art-tools\blender-4.5.3-windows-x64\blender.exe'
& $blender -b --python-exit-code 1 --python tools/art/build_photoreal_rider.py
& $blender -b --python-exit-code 1 --python tools/art/validate_photoreal_source.py
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_photoreal_rider.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/capture_photoreal_rider.py -EngineRoot D:/sdk/UE_5.6 -Gui
```

仅改几何后可设置 `$env:CITYCOURIER_PORTRAIT_MESH_ONLY='1'` 再导入，完成后移除此环境变量；它不会重建表面材质。修改着色或贴图时必须完整导入。纹理缓存检查生成参数、生成脚本、上游输入和输出文件的 SHA-256，避免旧贴图与新参数混用。

人物已安装时，旧 `import_art.py` 会先路由到保护人物的导入流程，不再执行会重写共享骨架、动画和蓝图的旧人物导入段。这一路径用于人物更新；场景布置仍由独立脚本处理。

`portrait_baseline.json` 永久保留首次保护记录；`portrait_import_validation.json` 保留当前成功导入报告，二者均在源目录，换 worktree 或没有 `Saved` 的新检出仍可验证。每轮导入使用当前资源的写入前后快照检查骨架、动画、蓝图与挂点，允许其他开发者合法更新功能资源。

## 验证结果

- UE 完整导入和最后一次 mesh-only 导入均通过；12 个受保护包未被本轮改写。
- UE 项目回归 **66/66**，导入流程自测 **15/15**，已保留的免费源文件校验 **116/116**。
- 最终图片 **16/16**，运行 `20261018T124906Z-256e094c`，无捕获错误。逐图 PNG SHA-256 见 [捕获记录](capture-results.json)，汇总见 [验证数据](verification.json)。
- Blender 原参考姿势 50 骨未变；UE 与原版一致为 51 骨（含导入的场景根）。源网格 347,833 顶点、581,976 三角形，零未绑定顶点。
- 12 张动作取样图中，显示姿势与压缩动画最大位置误差约 `0.0000212 cm`；实际骑乘的新旧骨骼在车辆坐标中的最大差为 `0 cm`。原生奔跑速度均为 `700 cm/s`，实际移动均超过 8 米。
- **既有骑姿挂点偏差仍存在**：原版与新版均为髋部约 15.53 cm、握把约 16 cm、脚踏约 17.09 cm。原生姿势和新旧一致性通过，不代表绝对握把/脚踏对齐通过。该偏差来自原有挂载变换，本轮没有修改游戏逻辑或旧动作。

UE 项目回归记录：`Saved/Validation/Automation-20261018-205042-3422f232/Report/index.json`。源文件中保留完整导入验证数据，`Saved` 中保留逐轮运行与日志。

## 来源与边界

CharMorph-Vitruvian 固定版本为 `c252d1fc7f00ba59d87b66b3d32fa3471fc6ee44`；上游源文件、配置中的 CC0 声明与 SHA-256 记录在 `AssetsSource/CityCourier/External/CharMorphProbe`。MakeHuman 身体、服装与皮肤的来源保存在 `External/MakeHumanPack`。见 [资产清单](../asset-manifest.csv)。

皮肤实现参考 [Epic Subsurface Profile 文档](https://dev.epicgames.com/documentation/en-us/unreal-engine/subsurface-profile-shading-model-in-unreal-engine)；源头部来自 [CharMorph-Vitruvian](https://github.com/Upliner/CharMorph-Vitruvian/tree/c252d1fc7f00ba59d87b66b3d32fa3471fc6ee44)。项目不依赖 MetaHuman。

当前没有新增面部表情、眨眼或口型骨架。近景发丝网格用于单个主角，尚未对大量同模人物、不同硬件和各级 LOD 做性能验收。这些边界不能通过漂亮的静态截图推断已经解决。
