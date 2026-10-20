# 蹲姿与匍匐动作

按住左 Shift 趴下，配合 WASD 以 60 cm/s 匍匐移动；松开后在头顶空间足够时起身。右 Shift 奔跑，Ctrl 蹲下及蹲行。低顶阻挡起身时继续保持趴姿，移出遮挡后自动起身。趴下时不能跳跃、出拳或上车。

## 动画与来源

复用项目保留的 Mesh2Motion CC0 动作库，版本 `653a9698f5f315523072f1c3ff3496100401317b`。源 GLB 和 SHA-256、导出 FBX 校验、UE 压缩误差及循环接缝检查保存在 `AssetsSource/CityCourier/posture_actions.json`。

| 资源 | 保留的源动作 | 循环时长 |
| --- | --- | --- |
| A_CrouchIdle | Crouch_Idle | 2.0 s |
| A_CrouchWalk | Crouch_Walk | 1.2 s |
| A_ProneIdle | Crawl 的固定待机姿势 | 1.5 s |
| A_ProneCrawl | Crawl | 1.5 s |

动作按当前角色的骨长进行 FK 转移，根节点固定、原地播放，位移由 CharacterMovement 处理。源文件为 `Blender/CourierPostures.blend`，导出位于 `Exports/PostureActions`。导入脚本核对现有角色网格、骨架和其他动画包的哈希，新增动画不改动这些资源。

## 运行时

原来的四段站立/骑乘动画继续由原数组提供，新姿态由独立的四段动画数组提供，并经过原生动画代理的交叉混合。蹲起时刷新网格世界变换；美术角色每帧按当前胶囊半高定位网格，避免站立高度覆盖趴姿而使身体下沉。

趴姿使用缩短的角色胶囊，以及覆盖头脚长度的空间检查。进入、转向和移动前检查身体占用范围；起身由 CharacterMovement 的扩张碰撞检查决定。动画按平地制作，未增加坡地手肘和膝盖 IK。

## 重建与检查

```powershell
.dependencies/art-tools/blender-4.5.3-windows-x64/blender.exe --background --python tools/art/build_posture_actions.py
./scripts/Build.ps1 -Target Editor -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_posture_actions.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/build_character_actions.py -EngineRoot D:/sdk/UE_5.6
./scripts/Test.ps1 -Filter CityCourier.CharacterActions -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/capture_posture_actions.py -Gui -EngineRoot D:/sdk/UE_5.6
```

`PostureTests.cpp` 检查头脚空间、起身受阻、动作互斥、暂停/界面输入、Shift 映射和实际角色每帧更新后的渲染骨骼。PIE 脚本通过现有 Enhanced Input 触发趴下、匍匐、站起和蹲下，记录移动速度、动画状态与头部离地高度；数据在 `Saved/PostureActions/results.json`。

## 2026-10-20 验证记录

- 角色动作组最终通过 27/27：`Saved/Validation/Automation-20261020-152538-e9c14894/Report/index.json`。
- 实际 PIE 录制通过：`Saved/Validation/EditorScript-20261020-152735-6f55c2ae`。匍匐速度 60 cm/s，记录的头部离地高度 18.28–31.34 cm，起身和蹲下检查通过。[状态记录](posture-actions/results.json)。输入通过生产 Enhanced Input 注入，Shift 的物理键映射另由自动化测试核对。
- 下沉回归测试在修复前失败：`Automation-20261020-152056-232d1c53`。修正美术角色固定站立偏移及蹲起时的网格世界变换刷新后通过。

![趴姿待机](posture-actions/prone-idle.png)

![实际匍匐移动](posture-actions/prone-crawl.png)

![蹲姿](posture-actions/crouch.png)
