# 自然待机与跑跳衔接

正式人物的四段循环绑定现在是 `A_IdleRelaxed / A_Jog / A_Sprint / A_Ride`。旧 `A_Idle` 保留，未覆盖人物模型、骨架、骑乘或拳击动作。

后续针对空中姿态和起跳覆盖中断的修正见 [跳跃姿态修正](jump-poses.md)，包括重做的起跳/腾空、下降伸腿和 98 项最终回归。

可参考的开放动作库：

- [Quaternius Universal Animation Library](https://quaternius.com/packs/universalanimationlibrary.html)：CC0，覆盖待机、慢跑、快跑、八方向移动等。完整版有 120 多段动作，免费 Standard 版仅包含其中约 60–70%；不要把完整目录当作全部免费。
- [Mesh2Motion 源美术仓库](https://github.com/Mesh2Motion/mesh2motion-assets)：CC0，提供骨架和可编辑动画源文件。本次直接使用项目已经保留的 `mesh2motion-app` 动作导出，无新下载。

本次待机来自保留版本 `653a9698f5f315523072f1c3ff3496100401317b` 的 `human-addon-animations.glb / Idle_Subtle`。许可证、下载地址与哈希在 `AssetsSource/CityCourier/External/Mesh2MotionProbe/sources.json`。源动作绝对姿势偏宽站距、半蹲和握拳，因此只迁移其小幅动作变化，再调整肩膀、肘部、手指与重心；固定双脚并闭合循环。新动作 2.533 秒、30 Hz，77 个采样，根骨无位移。

可编辑文件：`AssetsSource/CityCourier/Blender/CourierIdle.blend`；导出在 `Exports/RelaxedIdle`；参数和导入证据在 `relaxed_idle.json`。生成器从正式 `CourierPhotoreal.blend` 读取角色，不重新生成旧模型。导入校验 23 个既有角色/蓝图包哈希未变，允许修改的只有新待机和正式动画蓝图。

![旧站姿与新站姿，左旧右新，Blender 形体预览](locomotion/idle-comparison.png)

[正式人物原生待机正面](locomotion/native-idle-front.png)、[侧面](locomotion/native-idle-side.png) 来自真实 PIE 人物与动画蓝图，不是单节点播放。侧面截图头盔顶部超出画幅，不作为头盔完整性验收。

运行时修复：

- 慢跑与快跑切换延续归一化步伐周期，过渡中的旧动作按原倍率推进。
- 落地保留水平速度，仍保留原有 0.12 秒地面攻击/再次起跳保护。
- 移动落地混合持续的步伐与落地压缩，避免全身立刻切成双脚站定；静止落地仍使用完整落地动作。
- 跑跳动作切入为 0.10 秒，切出为 0.12 秒；已有停止动作切入仍为 0.15 秒。拳击播放和命中时间保持原参数。

回归结果：

| 检查 | 修改前 | 修改后 |
| --- | --- | --- |
| 指定慢跑转快跑周期后的双脚误差 | 87.047 / 59.184 cm | < 0.001 cm |
| 700 cm/s 奔跑后首次落地速度 | 0 cm/s | 700 cm/s |
| 指定落地时刻相对持续奔跑的最大脚位偏差 | 38.836 cm | 12.358 cm |

这些是固定场景的骨骼回归测量，不代表所有速度或落地阶段的最大误差。先失败报告：`Saved/Validation/Automation-20261019-145909-fcdff963/Report/index.json`。最终完整 **96 项**通过：`Saved/Validation/Automation-20261019-151009-fc374d34/Report/index.json`，包括拳击实际接触姿态和跳跃输入测试。

[原生 PIE 记录](locomotion/native-results.json) 确认实际输入先慢跑、再快跑、再移动跳跃，观察到 `A_JumpStart / A_JumpLoop / A_Land`，跳起约 90 cm，落地后最低水平速度 700 cm/s。该记录包含逐帧骨骼位置，没有在跑跳测量期间截图，也不声称自动判断美术自然程度。待机压缩后脚漂移 < 0.001 cm，循环端点误差 < 0.001 cm。

重建与验证：

```powershell
.dependencies/art-tools/blender-4.5.3-windows-x64/blender.exe --background --python-exit-code 1 --python tools/art/build_relaxed_idle.py
./scripts/Build.ps1 -Target Editor -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_relaxed_idle.py -EngineRoot D:/sdk/UE_5.6
./scripts/Test.ps1 -Filter CityCourier -Headless -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/capture_locomotion_continuity.py -Gui -EngineRoot D:/sdk/UE_5.6
```

站姿仍使用现有衣服与皮肤权重；本次没有重新制作人物网格或全身动作捕捉。最终自然程度需结合游戏中的镜头和玩家观感继续评估。

Windows Development 编译、烘焙和打包通过：`Saved/Validation/Package-Development-20261019-151345-17ffc9fd/package.log`。可运行文件为 `Artifacts/Windows/Development/CityCourier.exe`。
