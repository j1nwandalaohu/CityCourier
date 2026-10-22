# Epic 角色动作接入记录

2026-10-22：已连接正式 `BP_CourierCharacter`，最新 Editor 构建和 57 项相关 UE 自动化全部通过；独立审查的三项 Important 已修复并验证，两项 Minor 留待后续。用户已授权本次 Editor 构建、资源生成和自动化测试；未打包、未启动游戏窗口。

## 实际接入

| 来源 | 接入内容 | 目标资源/代码 |
| --- | --- | --- |
| Game Animation Sample | 骑手骨架上的 Motion Matching、地面/空中 Chooser、跳跃落地、Root Motion 翻越 | `Content/CityCourier/Animation/Epic/`、`Player/Epic/CourierTraversalComponent.*` |
| Stack O Bot 参考 | 明确的可抓取能力、PhysicsHandle、接触后才触发的按钮、骑手人体手脚对位 | `CourierObjectInteractionComponent.*`、`CourierContactRig.*`、`World/CourierGrabbable.*`、`CourierInteractionButton.*` |
| Content Examples 参考 | 骑手自己的 PhysicsAsset、上半身局部模拟、PhysicalAnimation 驱动与恢复 | `CourierPhysicalReactionComponent.*`、`PHYS_Courier` |

GASP 实际迁移 1,397 个源包，约 1.14 GiB；保留原包路径和迁移前后哈希。生成 930 个骑手资源，包含 885 个序列、34 个蒙太奇以及配置、图、数据库、Rig 和 PhysicsAsset。Stack O Bot 与 Content Examples 的机器人/示例角色、关卡和控制器没有替换骑手；其交互与局部物理设计以当前骑手骨架重新实现。

正式入口使用 `/Game/CityCourier/Animation/Epic/DA_CourierEpicAnimation`。旧动画代理保留为校验失败时的兼容路径，新配置通过后使用标准 AnimGraph；现有攻击时间线、配送、交互优先级和电动车控制继续拥有各自状态。编辑器冷启动索引未就绪时暂保留旧图，在安全的地面动作状态重试。

新图包含地面/空中 Motion Matching、既有骑车/蹲伏/匍匐姿态、Pose History、`FullBodyAction`、Inertialization 和人体 Control Rig。攻击期间关闭接触修正，保持现有拳击接触姿态和一次命中的规则。

骑手骨架根节点有 100 倍单位换算。生成器校正骨盆局部位移、保存目标参考根缩放、固定参考根锁定，并关闭会再次放大位移的 normalized root motion。数据库排除瞄准偏移、基础叠加姿态及不适用的蹲伏/翻越片段。生成输出移除源蓝图通知和指向原版数据库的 Pose Search BranchIn 通知，保留原生排除范围及 Motion Warping。源动画与重定向清单仅作编辑器数据保存。

## 行为边界

- 翻越使用已下载的约 1 m 动画，候选高度限制为 80–110 cm。检查障碍、顶部、出口和头顶通行；移动车辆走原有跳跃/站立移动基座逻辑。目标销毁、移动、受击、UI 和挂载会取消翻越。
- 可抓取物必须是明确的 `CourierGrabbable`，默认质量 10 kg、上限 20 kg；E 拿取/放下。PhysicsHandle 保留物理模拟，释放或中断恢复碰撞。
- 按钮使用真实 `hand_l/hand_r` 骨骼位置，接触误差不超过 5 cm 才提交一次；触点遮挡、失效、距离变化会取消。恢复从实际接触时刻开始。
- 手脚目标在游戏线程取样。Rig 只消费组件空间快照，不在动画工作线程查询世界；离地、匍匐、骑车、UI 和动作占用会释放权重。手腕采用交互点朝向，落脚采用坡面倾角；摆腿按输入动画的抬脚高度释放修正，骨盆补偿按地面高度差计算。
- 物理反应只作用于上半身子树；根/骨盆继续由角色移动控制。连续冲量总预算 2,500，约 0.45 秒恢复；保存并恢复原有模拟、混合和碰撞状态。

验证场景：`/Game/CityCourier/Maps/L_EpicCharacterValidation`，包含走跑区域、90 cm 翻越、105 cm 登台、台阶、可抓取物、按钮、拳击靶和电动车。生成脚本再次运行只校验回执；检测到人工修改会拒绝覆盖。

## 重现与证据

所有 UE 命令顺序执行，构建成功后再执行依赖该模块的编辑器脚本和测试。

```powershell
./scripts/Build.ps1 -Target Editor
./scripts/Run-EditorScript.ps1 -Script Content/Python/build_epic_courier_animation.py -Gui
./scripts/Run-EditorScript.ps1 -Script Content/Python/build_epic_courier_graph.py
./scripts/Run-EditorScript.ps1 -Script Content/Python/build_epic_courier_physics.py
./scripts/Run-EditorScript.ps1 -Script Content/Python/build_epic_character_validation.py
./scripts/Test.ps1 -Filter CityCourier.EpicAnimation
./scripts/Test.ps1 -Filter CityCourier.EpicCharacterIntegration -Headless
./scripts/Test.ps1 -Filter CityCourier.EpicInteraction -Headless
./scripts/Test.ps1 -Filter CityCourier.Traversal -Headless
./scripts/Test.ps1 -Filter CityCourier.PhysicalReaction -Headless
./scripts/Test.ps1 -Filter CityCourier.CharacterActions -Headless
./scripts/Test.ps1 -Filter CityCourier.Interaction -Headless
./scripts/Test.ps1 -Filter CityCourier.Scooter -Headless
./scripts/Test.ps1 -Filter CityCourier.VehicleTraversal -Headless
```

`EpicAnimation` 的离屏截图测试需要 RHI，因此该组不加 `-Headless`。它通过 SceneCapture 渲染到纹理，不打开游戏窗口。证据保存在 `Saved/Validation/EpicSamples/Contact/{before.png,contact.png,state.json}`。完整流程测试使用正式蓝图，检查实际按钮接触、根运动跨过障碍、抓放、局部物理及挂载切换。物理用例还推进真实物理世界，检查刚体运动和胶囊位置。

离线状态由 `Content/Python/validate_epic_character_integration.py` 生成至 `Saved/Validation/EpicSamples/integration-status.json`：逐项验证资源哈希、最新 Editor 构建及其后的测试结果，区分 assets/code/build/runtime，不把脚本存在当作功能通过。Python 迁移测试 6 项覆盖冲突不覆盖、重复运行、路径逃逸、UE 重新序列化回执及人工编辑检测。

来源清单位于 `docs/art/asset-manifest.csv`，源审计、迁移和生成回执位于 `docs/art/epic-character-samples/`。Epic 发布内容遵循获取时的 Epic/Fab 条款，不标为 CC0；生成的动画行可追溯到实际源包。Fab 来源：<https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016>。

## 实施取舍

- 在用户指定的现有功能分支顺序实施，保留无关未跟踪文件；变更共用该分支。
- 官方下载缺少两个 Land_Roll 片段所需的 Foley 通知，排除这两个片段；本次没有翻滚落地变体。
- UE 迁移会重新序列化包，分别保存源和目标哈希；后续重复运行依赖回执识别人工修改。
- UE 5.6 IK 批处理依赖编辑器 UI 子系统，初始重定向使用隐藏 Editor 的脚本模式；没有启动游戏。
- 中途导出已产生的 919 个动画按审计选择和实际哈希恢复接管，再统一清理和索引；没有重新覆盖原始示例包。
- 第一阶段保留完整的源依赖包供审计和重生成，正式配置仅以编辑器数据引用源清单；暂不删除这些已导入源文件。
- 授权到达前暂停依赖新原生类的生成和后续玩法任务；授权后已继续并完成，暂停期间未声明未执行测试通过。
- 使用自动化游戏 World 和离屏 SceneCapture 满足本轮不启动游戏窗口的约束；未进行主观交互式 PIE 预览。
- Tasks 2–8 因共享动画图、配置和组件接口，在阶段测试通过后统一提交；完整实现 diff 较大，阶段过程由验证记录保留。

## 本轮验证

最新构建：`Saved/Validation/Build-Editor-20261022-182724-605ececf/build.log`。

| 过滤组 | 通过数 |
| --- | ---: |
| EpicAnimation（含 RHI 离屏） | 10 |
| EpicCharacterIntegration | 2 |
| EpicInteraction | 3 |
| Traversal | 3 |
| PhysicalReaction | 2 |
| CharacterActions | 27 |
| Interaction | 2 |
| Scooter | 3 |
| VehicleTraversal | 5 |
| 合计 | 57 |

另外，Python 迁移测试 6/6、Epic 脚本 AST 和 `git diff --check` 通过。全部 919 个生成动画的骨架、通知及硬依赖审计通过；四个生成器再次运行，930 个生成包及正式蓝图/验证地图共 932 个文件保持字节一致。可保留的汇总证据见 `docs/art/epic-character-samples/{validation,target-clip-audit,idempotence}.json`；详细引擎报告在 `Saved/Validation/`。

## 最终审查与边界

独立审查覆盖 c85bf4b..a69f7a6，未发现 Critical。三项 Important 在同一轮修复：

- 资源模型测试增加 WITH_EDITOR 守卫，避免 Development Game 编译 editor-only 字段；本轮只构建 Editor，未构建或打包 Game。
- 正式角色接地后通过现有 FullBodyAction 播放站立/移动 GASP 落地片段，攻击继续优先。JumpLanding 先复现无落地蒙太奇，再验证片段来源、时间推进、实际骨盆姿态变化与槽释放。
- 四个生成器缺少本地回执时读取 docs/art/epic-character-samples 下的已提交回执，仍逐项验证资产哈希；本地坏回执不回退绕过。移开本地 gasp/migration/generation 回执后，四个生成器复用 932 个文件，全部保持字节一致。

两项 Minor 延后处理：

- 验证/重定向中途失败时，已有进度回执尚不能完整自动恢复，可能需要人工核实后恢复；成功生成与重复复用不受影响。
- 持物时趴下仍保留 PhysicsHandle，而手部 IK 淡出；后续应限制该姿态切换或自动放下物体。

没有执行交互式 PIE、完整主观动作预览、打包或运行游戏窗口。离屏按钮画面和自动化骨骼/物理断言不能替代完整主观动作质量验收。未包含多人、移动障碍翻越或全身布娃娃恢复。
