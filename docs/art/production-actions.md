# 正式跑步与拳击动画

WASD 使用 `A_Jog`（420 cm/s），Shift 使用 `A_Sprint`（700 cm/s）。两个动画来自此前验证过的 Mesh2Motion 候选动作，播放倍率按自然速度 473.2 / 715.9 cm/s 适配。正式蓝图和原生动画求值器使用相同四个槽位：Idle、Jog、Sprint、Ride。原 `A_Walk`、`A_Run` 作为历史资源保留；骑乘动画、正式角色网格与骨架未替换。

| 输入 | 正式素材 | CC0 来源动作 | 时长 | 伸展峰值 |
| --- | --- | --- | --- | --- |
| 左键首击 | A_LightPunch_L | Punch_Jab | 0.55 s | 0.18 s |
| 连击窗口内再次左键 | A_LightPunch_R | Punch_Cross | 0.55 s | 0.18 s |
| 右键重拳 | A_HeavyPunch_R | Melee_Hook | 0.95 s | 0.40 s |

素材使用仓库保留的 Mesh2Motion CC0 动作，版本和许可同 [人物动作试验](character-probe/README.md)。基于原有 50 骨骼重定向流程生成，根骨不产生位移；按真实拳部前伸峰值分段重定时，以匹配既有命中窗口。轻拳按 60 fps 输出 33 个间隔，重拳为 57 个间隔，避免 30 fps 无法准确表达 0.55 / 0.95 秒。拳套挂点、命中距离、遮挡和界面输入规则沿用现有实现。

可编辑源文件为 `AssetsSource/CityCourier/Blender/CourierCombat.blend`，FBX 在 `Exports/ProductionActions`，参数记录为 `production_actions.json`，正式 UE 资源在 `/Game/CityCourier/Art/Characters/`。导入脚本仅处理这五段动作并重建正式动画蓝图绑定，校验网格、骨架与骑乘资产文件未变。主美术导入流程也会在这些源文件存在时恢复正式动作绑定。

重建：

```powershell
.dependencies/art-tools/blender-4.5.3-windows-x64/blender.exe --background --python tools/art/build_production_actions.py
./scripts/Build.ps1 -Target Editor -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_production_actions.py -EngineRoot D:/sdk/UE_5.6
./scripts/Test.ps1 -Filter CityCourier -Headless -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/capture_production_actions.py -EngineRoot D:/sdk/UE_5.6 -Gui
```

完整回归 69 项通过：`Saved/Validation/Automation-20261018-191759-c0c14da3/Report/index.json`。新增检查覆盖实际生产蓝图的四段绑定、三段真实拳击的骨架与时长、伸展峰值处于命中窗口，以及最终动画姿态和测试靶真实命中。未用走路／骑乘动画代替拳击素材。

正式街区实际 PIE 输入录制通过：`EditorScript-20261018-192236-4eda066b`。通过已有 Enhanced Input 绑定注入移动、Shift、左右键，读回速度分别为 420 / 700 cm/s，观察到左右轻拳连击及重拳状态。不是直接将动作文件指定给截图角色。保留 [慢跑](production-actions/jog.png)、[快跑](production-actions/sprint.png)、[左轻拳](production-actions/light-left.png)、[右轻拳](production-actions/light-right.png)、[重拳](production-actions/heavy.png) 和 [状态记录](production-actions/results.json)。录制将角色放到空旷道路，避免出生点电动车挡住移动；截图回读可能阻塞渲染，因此先缓存第二次轻拳输入再截取第一拳。该录制验证输入接口与实际渲染，未声称覆盖每种物理鼠标／键盘设备。

Windows Development 重新编译打包成功：`Saved/Validation/Package-Development-20261018-192419-40cdf3e1/package.log`。116 个保留外部源文件的 SHA-256 校验通过；本轮没有新增下载或付费素材。

打包版 `CharacterActions` 烟雾验证也通过，`Saved/Validation/PackagedProductionActions-Final/result.json` 的 error 为空；确认真实拳击可准入、动作可结束，物理跳跃、空中界面重力及暂停输入隔离未回归。待交付素材数量由十段减少为七段，三段拳击均已被烘焙并加载。该旧验证器仍不对全部十段新动作给出视觉验收；本轮五项画面与输入证据见上面的 PIE 录制。

本次范围是慢跑、快跑和三段出拳；专用跳跃、落地、受击及起停七段动画仍待制作。动作使用现有衣服权重，极端肩肘姿态仍可能存在服装穿插，需要后续美术精修。
