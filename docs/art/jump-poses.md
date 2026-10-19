# 跳跃姿态修正

本轮针对空中身体/腿部突然变形、腾空姿势像站立的问题。上一轮速度和步伐回归通过，未能覆盖这两个实际画面问题。

实机逐阶段采集显示：旧起跳动作只有 0.18 秒，但自动淡出持续 0.12 秒；起跳还没播完就开始退回基础跑步。在约 0.133 秒时活动起跳蒙太奇已经为空，到空中动作淡入完成前出现跑姿。另一个问题在素材：旧腾空动作混入大量站姿，并覆盖了原有手臂动作，形成手伸在前面、腿向下垂的姿势。

现在起跳保持到腾空动作接替，同时设置蒙太奇资产与活动播放实例的自动淡出标志。引擎初始化会复制该标志，只修改播放后的资产无效。回归测得衔接期间最低覆盖权重由 **0 变为 1**。

三段新素材来自项目保留的 CC0 `Jump_2`，无需新增下载：

| 动作 | 时长 | 用途 |
| --- | --- | --- |
| A_JumpStart | 0.18 s | 从放松站姿进入收腿 |
| A_JumpLoop | 0.40 s 循环 | 小幅保持收腿姿态 |
| A_JumpFall | 0.20 s，结束后保持 | 垂直速度低于 -80 cm/s 时伸腿准备落地 |

目标骨架按自身骨长做 FK 姿势迁移，移除源动作的位移轨迹；空中不再做逐帧地面抬升或脚部固定。手腕与手指采用放松姿态，源动作幅度适当减弱，避免过深收腿。人物物理跳跃高度、重力、水平速度和原有落地/拳击资源不变。可编辑源文件 `AssetsSource/CityCourier/Blender/CourierJump.blend`，FBX 在 `Exports/JumpActions`，源版本、许可证、校验和及导入检查见 `jump_actions.json`。

实机对比（相同侧面镜头；通过游戏输入触发，然后在指定游戏时间暂停截图）：

| 阶段 | 修正前 | 修正后 |
| --- | --- | --- |
| 起跳约 0.133 s | ![旧起跳](jump-poses/before-takeoff.png) | ![新起跳](jump-poses/after-takeoff.png) |
| 最高点约 0.433 s | ![旧空中姿势](jump-poses/before-air.png) | ![新空中姿势](jump-poses/after-air.png) |

[下降伸腿](jump-poses/after-descent.png)、[原地跳跃正面](jump-poses/stationary-air.png)。截图为正式人物和原生动画蓝图输出，不是单节点播放；暂停用于观察姿势，不能当作连续视频或帧率测量。

采集记录：[旧版](jump-poses/before.json)、[最终奔跑跳跃](jump-poses/after.json)、[最终原地跳跃](jump-poses/stationary.json)。它们均完成并保留实际输入、骨骼位置、蒙太奇、垂直速度和受保护资源检查。跑跳和原地跳跃均约 90 cm 高，跑跳落地后保持 700 cm/s。最终两次采集日志为 `EditorScript-20261019-155645-613fe1b5` 和 `EditorScript-20261019-155812-ff9b54c4`。

自动化先复现失败：`Automation-20261019-154034-5eb9ea46` 检出覆盖中断，`Automation-20261019-154649-e8d65937` 检出下降仍播放旧腾空循环。最终 **98 项完整回归通过**：`Saved/Validation/Automation-20261019-155558-2441543e/Report/index.json`。专用检查包含起跳覆盖、上升/下降姿态、长时间下落保持，以及真实骨骼输出与压缩后动作一致；保留原有拳击接触和移动落地检查。

素材导入验证根骨无漂移、循环闭合，压缩误差低于 1 mm；人物网格、共享骨架、待机、跑步、骑乘和拳击等其他包哈希未变。

重建时使用以下流程。旧 `build_pending_actions.py / import_pending_actions.py` 用于历史七段素材复现，会覆盖旧跳跃；如需运行它们，应最后运行本轮跳跃生成与导入，以恢复当前版本。

```powershell
.dependencies/art-tools/blender-4.5.3-windows-x64/blender.exe --background --python-exit-code 1 --python tools/art/build_jump_actions.py -- --export
./scripts/Build.ps1 -Target Editor -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_jump_actions.py -EngineRoot D:/sdk/UE_5.6
./scripts/Test.ps1 -Filter CityCourier -Headless -EngineRoot D:/sdk/UE_5.6
$env:CC_JUMP_POSES='1'
$env:CC_JUMP_PHASE='After' # 原地正面使用 Stationary
./scripts/Run-EditorScript.ps1 -Script Content/Python/capture_locomotion_continuity.py -Gui -EngineRoot D:/sdk/UE_5.6
```

正式动画参考与许可证沿用 [Mesh2Motion 源美术](https://github.com/Mesh2Motion/mesh2motion-assets)，实际保留的 GLB 源位于 `mesh2motion-app` 版本 `653a9698f5f315523072f1c3ff3496100401317b`，具体下载和哈希记录在 `External/Mesh2MotionProbe/sources.json`。

最终 Windows Development 编译、烘焙和打包完成：`Saved/Validation/Package-Development-20261019-160011-afe0476b/package.log`。
