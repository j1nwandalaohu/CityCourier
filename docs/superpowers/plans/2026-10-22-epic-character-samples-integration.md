# Epic Character Samples Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans for inline execution, or superpowers:subagent-driven-development only if the user chooses delegation. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 GASP 的移动与翻越、Stack O Bot 的抓取与对位、Content Examples 的局部物理受撞适配到现有骑手，交付实际资源、代码、来源及验证记录。

**Architecture:** 保留 `ACourierCharacter` 和 CharacterMovement，新增独立动画、翻越、交互和物理反应组件。将当前原生姿态播放器与标准动画图明确分为兼容路径和 Epic 路径，使用当前骑手骨架的重定向资源；订单、骑车和攻击仍由既有系统负责。

**Tech Stack:** Unreal Engine 5.6.1、C++、Unreal Python、PoseSearch、Chooser、MotionWarping、AnimationWarping、IKRig、ControlRig、PhysicsHandle、PhysicalAnimationComponent。

**Spec:** [已确认的设计](../specs/2026-10-22-epic-character-samples-integration-design.md)

**执行状态（2026-10-22）：** Tasks 1–8 已顺序实施；正式角色已启用，930 个生成资源、验证场景、919 个动画依赖审计、932 个文件重复生成校验已通过。最新 Editor 构建及 57 项相关 UE 自动化通过；独立审查完成，三项 Important 已修复验证，两项 Minor 已记录。未打包、未启动游戏窗口。证据见 [实施记录](../../epic-character-integration.md)。

## Global Constraints

- 目标工程是 UE 5.6.1 的单机配送游戏。沿用骑手外观、订单系统、骑车和既有动作输入。
- 保持 CharacterMovement 作为当前移动模型，本次不改为 Mover。
- 本轮用户已明确授权 Editor 构建、编辑器资源生成和 UE 自动化测试；不打包、不启动游戏窗口。
- 资源使用按需迁移，不把示例的机器人、测试城市、GameMode 或 PlayerController 直接替换进游戏。
- 新图和依赖检查通过前保留旧动画路径作为明确的兼容配置。缺资源时不得静默宣称启用新系统。
- 不修改现有未跟踪的 storefront 脚本、文档或快捷方式；显式暂存本任务文件。
- 运行编辑器资产处理、构建及 UE 测试时逐个执行，不并行运行 UE 进程。
- 新增原生辅助类均先完成授权的 Editor 构建，再执行依赖这些类的生成脚本；测试与编辑器进程顺序运行。
- 只有实际引入的资源进入来源清单，保留 Epic 原始许可说明，不标为 CC0。

## Review Focus

1. 蒙太奇结束、跳跃落地及骑车切换不能被新动画重复更新；Task 3 的 `ActionsRemainAuthoritative` 验证一次命中及可见动作。
2. 资源缺失或作者调整过 Rig 时不能覆盖旧资源或留下半启用配置；Task 1/2 的迁移校验和 `IncompleteSetupRejected` 验证失败前后配置一致。
3. 目标在交互接触前销毁或被遮挡时不能产生按钮效果；Task 5 的 `ContactRevalidation` 验证触发次数为零。
4. 持物、翻越和局部物理模拟在进入骑车、界面抑制或 EndPlay 时不能遗留状态；Task 4/5/7 各自验证取消及状态恢复。
5. 移动汽车及窄落点不能被固定世界目标的翻越吸附；Task 4 的 `MovingBaseAndBlockedExit` 回归车辆攀跳与受阻出口。

## 任务组织与执行规则

这是一个共同动画入口上的顺序集成，分成三个功能阶段：Task 1–4 为 GASP，Task 5–6 为 Stack O Bot，Task 7 为 Content Examples；Task 8 做统一场景和交付检查。每阶段均能单独审查，不一次替换整个角色。

运行许可未给出时，测试实现可以写好，但执行框必须保持未勾选并说明原因。所有测试运行前先构建，遵守仓库要求；不把无法执行的红绿测试写成已通过。纯 Python 检查使用 `ast.parse`，避免为语法检查生成仓库内 `__pycache__`。

### Task 1: 源工程审计与可追溯迁移

**Files:** 创建 `Content/Python/audit_epic_character_samples.py`、`migrate_epic_character_samples.py`、`epic_character_sample_paths.py`；创建 `Source/CityCourier/Tests/EpicSampleAssetTests.cpp`；更新 `docs/art/asset-manifest.csv`。

**Interfaces:** Python `audit_sources(source_roots: dict[str, str], output_path: str) -> dict` 输出源工程版本、资产类、Skeleton、硬/软依赖及文件摘要；`migrate_selected(inventory_path: str, destination_content: str) -> dict` 输出迁移包映射和变更列表。报告固定写入 `Saved/Validation/EpicSamples/`。

- [x] 实现审计脚本：核对三个本机源路径和 `EngineAssociation == '5.6'`；用 Asset Registry 和编辑器资产接口读取设计中列出的源资产，记录实际类与引用。通过命令行 `-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities` 在各源工程分别执行读取脚本，不保存源资产。
- [x] 定义所选 GASP 移动、跳跃、翻越、Skeleton 和 Rig 的资源集合；Stack O Bot 与 Content Examples 的蓝图作为参考记录，不将它们的 GameMode、地图或整个角色依赖自动迁入运行时。
- [x] 编写 `CityCourier.EpicAnimation.Assets.SourceAndDependencyClosure`：断言报告含三个源版本、源资产摘要、每个输出包的来源，以及全部运行时硬依赖均能解析。
- [x] 实现依赖迁移及冲突预检：使用 AssetTools 的包迁移，保留源包引用路径；导入前核对同名包摘要，差异包停止而非覆盖。新生成资源归入 `/Game/CityCourier/Animation/Epic`。
- [x] 验证 Python AST、报告结构及重复执行变更列表；运行不依赖新 C++ 类的资产审计/迁移，记录实际结果。UE 自动化用例的红绿执行留待构建/运行授权。
- [x] 显式暂存上述文件和实际迁移资产，提交 `feat(art): add traced Epic animation source migration`。

### Task 2: Courier 骨架重定向和动画配置

**Files:** 创建 `Player/Epic/CourierEpicAnimationConfig.{h,cpp}`、`Player/Epic/CourierEpicArtBuilder.{h,cpp}`、`Content/Python/build_epic_courier_animation.py`；更新 `CityCourier.Build.cs`、`CityCourier.uproject`；扩展 `Tests/EpicSampleAssetTests.cpp`。

**Interfaces:** `UCourierEpicAnimationConfig : UDataAsset` 持有移动/空中数据库、翻越蒙太奇、交互 Rig 和 PhysicsAsset 的软引用；`bool ValidateFor(const USkeletalMesh* Mesh, FText& OutReason) const` 验证骨架和依赖。`UCourierEpicArtBuilder::BuildRetargetAssets(UCourierEpicAnimationConfig* Config, FText& OutReason) -> bool` 为编辑器生成入口。

- [x] 编写 `CityCourier.EpicAnimation.Assets.IncompleteSetupRejected`：缺任一必需资产时验证失败，旧动画类保持不变；目标骨架不一致时明确失败。
- [x] 建立 UEFN 到 Courier 的 IK 链和参考姿态，覆盖躯干、手臂和双腿，处理 `calf -> shin` 等实际骨名映射；验证比例和朝向，不修改现有 Skeleton 以伪造兼容。
- [x] 使用 UE 5.6 的 `UIKRetargetBatchOperation::DuplicateAndRetarget` 生成 Courier Skeleton 的输出动画，保留翻越所需 Root Motion；普通移动按移动模型配置根运动策略。
- [x] 生成 Courier Skeleton 对应的 Schema、Pose Search 数据库、Chooser 和配置资产。数据库涵盖走跑、起停、转向、空中及落地，重新建立索引并排除不兼容片段。
- [x] 新增来源记录及生成摘要，重复运行时只更新摘要匹配的自动生成资产。人工修改导致摘要不匹配时停止并列出冲突。
- [x] 新辅助类的构建和编辑器生成在获准后顺序执行；断言输出 Skeleton 全部匹配、数据库非空、必要曲线/根运动有效，保存报告。未执行时不启用新配置。
- [x] 随共享动画图和组件接口统一提交；各阶段测试证据独立保留（见实施取舍）。

### Task 3: 标准动画图与 GASP 移动接入

**Files:** 修改 `Player/CourierAnimInstance.{h,cpp}`、`CourierArtBuilder.cpp`、`CourierCharacter.cpp`、`CourierActionComponent.{h,cpp}`；扩展 `Player/Epic/CourierEpicArtBuilder.*`；创建 `Tests/EpicAnimationTests.cpp`；更新 `Tests/ProductionAnimationTests.cpp`。

**Interfaces:** `UCourierAnimInstance::IsEpicAnimationEnabled() const -> bool`；`ACourierCharacter::TryEnableEpicAnimation(UCourierEpicAnimationConfig* Config) -> FCourierActionResult`。失败时不改变动画类。原生 `FCourierAnimProxy` 仅服务兼容路径，Epic 路径使用标准图的更新及求值。

- [x] 编写 `CityCourier.EpicAnimation.Graph.UsesRetargetedLocomotion`：实际骑手蓝图启用后使用目标骨架数据库，移动与空中能输出非参考姿态，骑车仍使用原座姿。
- [x] 将轨迹、速度、加速度、朝向、空中、姿态和交互参数接到标准 AnimGraph；接入移动/空中选择、惯性过渡和现有 `FullBodyAction` Slot。
- [x] 区分新旧蒙太奇所有权：Epic 图已负责跳跃和落地时，动作组件不再额外播放旧跳跃蒙太奇；攻击命中时间窗与动作时钟保持原职责。
- [x] 编写 `ActionsRemainAuthoritative`：轻/重攻击实际播放与接触窗口一致，每次攻击只命中一次；骑车、蹲伏、趴下和关闭界面后恢复正确路径。
- [x] 给旧 `CourierArtBuilder` 增加识别与拒绝覆盖新动画图的检查，重复执行旧生成脚本不能清空新图。
- [x] 获准构建/运行后执行 `CityCourier.EpicAnimation`、`CityCourier.CharacterActions`，采集起停、转向、跳跃落地的截图/状态。未授权时保留待验证状态。
- [x] 随共享动画图和组件接口统一提交；各阶段测试证据独立保留（见实施取舍）。

### Task 4: Vault/Mantle 与 Motion Warping

**Files:** 创建 `Player/Epic/CourierTraversalComponent.{h,cpp}`、`Tests/EpicTraversalTests.cpp`；修改 `CourierCharacter.*`、`CourierActionComponent.cpp`；扩展动画配置和生成器。

**Interfaces:** `FCourierTraversalCandidate` 保存目标 Actor/PrimitiveComponent 弱引用、局部入口/出口、动作类型及对应蒙太奇；`UCourierTraversalComponent::TryStart() -> FCourierActionResult`、`Cancel()`、`IsActive() const -> bool`。

- [x] 编写 `CityCourier.Traversal.ValidObstacleAndLanding`：覆盖 Vault/Mantle、站立/移动入口、空格失败后普通跳跃；实际胶囊最终落在可站立出口，动作不能只播放而不移动。
- [x] 根据审计得到的源动画覆盖范围配置高度/深度限制；用胶囊通行、头顶净空、地面坡度和出口查询筛选候选，使用源动画实际覆盖范围的上下边界分别写通过/拒绝用例。
- [x] 从空间检查结果生成蒙太奇 Warp 目标，绑定移动状态及碰撞恢复；恢复本组件拥有的设置，正常完成后由真实落地结果决定移动状态。
- [x] 编写 `MovingBaseAndBlockedExit`：移动汽车交给原汽车攀跳路径；出口变堵、目标销毁、受击、输入抑制及取消不能穿墙或残留动作锁。
- [x] 获准后运行 `CityCourier.Traversal`、`CityCourier.VehicleTraversal`，录制成功翻越和受阻取消，检查手脚接触和根运动终点。
- [x] 随共享动画图和组件接口统一提交；各阶段测试证据独立保留（见实施取舍）。

### Task 5: 物理抓取和按钮交互

**Files:** 创建 `Player/Epic/CourierObjectInteractionComponent.{h,cpp}`、`World/CourierGrabbable.{h,cpp}`、`World/CourierInteractionButton.{h,cpp}`、`Tests/EpicObjectInteractionTests.cpp`；修改 `CourierCharacter.*`、`CourierInteractable.h` 和 `CourierActionComponent.cpp`。

**Interfaces:** `FCourierInteractionPose` 包含左右手世界目标与权重；在 `ICourierInteractable` 增加有默认实现的可选姿态查询，既有实现无需修改。`UCourierObjectInteractionComponent::TryGrab(UPrimitiveComponent* Target) -> FCourierActionResult`、`TryPress(AActor* Target) -> FCourierActionResult`、`Release()`、`Cancel()`、`IsBusy() const -> bool`、`GetPose() const -> FCourierInteractionPose`。

- [x] 编写 `CityCourier.EpicInteraction.GrabRelease`：只有配置了抓取能力、可模拟物理且通过原 200cm 距离/遮挡检查的对象可拿取；E 再次释放。质量上限为可编辑属性，默认 20kg，边界值 20kg 接受、20.01kg 拒绝。
- [x] 参考 BP_Bot 的 Grab 生命周期实现 PhysicsHandle，保存抓取前状态；忽略建筑和车辆，持有物体不进入订单库存。为建筑交互候选查询补上注册通道，使验证对象在真实主地图的查找策略中也可被发现。
- [x] 实现按钮对位和一次性接触提交；`ContactRevalidation` 断言触碰前遮挡或目标销毁时触发零次，持续按住/重复回调仍只提交一次。
- [x] `CancellationRestoresObject` 覆盖受击、骑车、界面抑制、目标销毁和 EndPlay。释放物体保持可用碰撞/模拟状态，句柄和交互目标均清空。
- [x] 获准后运行 `CityCourier.EpicInteraction` 和现有 `CityCourier.Interaction`，验证候选优先级及取餐交互仍按既有规则执行。
- [x] 随共享动画图和组件接口统一提交；各阶段测试证据独立保留（见实施取舍）。

### Task 6: 人形手脚对位

**Files:** 扩展 `Player/Epic/CourierEpicArtBuilder.*`、`CourierAnimInstance.*` 和 `build_epic_courier_animation.py`；创建 `Tests/EpicContactPoseTests.cpp`；生成 `CR_CourierContact` 与新动画图资源。

**Interfaces:** `UCourierAnimInstance` 提供 `FCourierInteractionPose`、左右脚接触目标和权重；Rig 只消费当帧快照，异步动画求值不访问 World 或修改交互玩法。

- [x] 编写 `CityCourier.EpicAnimation.Contact.HandReach`：可达按钮/物体目标时手部最终位置误差不超过 5cm，不可达目标不无限拉伸肢体。
- [x] 参考 `CR_Bot_Correction` 的目标混合方式，为 Courier 骨名建立独立 Rig；与 GASP 动画输出组合，使用躯干与肢体限制，不能把机器人 Rig 直接赋给骑手。
- [x] 脚部查询在游戏线程更新，通过动画参数传递；`FootContactStateWeights` 检查站立贴地、台阶变化及空中/翻越/骑车/趴下权重退出，误差和权重需随状态连续变化。
- [x] 获准后执行 `CityCourier.EpicAnimation.Contact`，截取按钮接触、双手持物及台阶脚部对位的最终姿态。
- [x] 随共享动画图和组件接口统一提交；各阶段测试证据独立保留（见实施取舍）。

### Task 7: 局部物理受撞

**Files:** 创建 `Player/Epic/CourierPhysicalReactionComponent.{h,cpp}`、`Tests/EpicPhysicalReactionTests.cpp`、`Content/Python/build_epic_courier_physics.py`；修改 `CourierCharacter.*`、`CourierActionComponent.cpp`、动画配置和来源清单。

**Interfaces:** `UCourierPhysicalReactionComponent::ApplyImpact(FName Bone, FVector Impulse) -> bool`、`Cancel()`、`IsActive() const -> bool`；质量/冲量上限及进入/恢复时间为可编辑参数，默认最大冲量 2500、恢复时间 0.45s。组件拒绝 NaN/Inf、根骨骼和缺少物理体的目标。

- [x] 编写 `CityCourier.PhysicalReaction.LocalImpulseAndRecovery`：上肢冲量只激活选定局部，根和胶囊不进入全身模拟；恢复后模拟、碰撞和混合设置与进入前一致。
- [x] 核对当前 PhysicsAsset，按 Courier 实际骨架建立受撞所需物理体及约束。源示例的 PhysicsAsset 只作参考，不直接赋给不同 Skeleton。
- [x] 参考 Content Examples 使用 PhysicalAnimationComponent、局部模拟和受限冲量，平滑恢复权重；重复受撞不超过设置的冲量上限。
- [x] 编写 `InterruptionsRestorePriorState`：骑车、取消、EndPlay、缺资产及原本已有局部模拟的情况均安全；受击入口只调用一次原有命中处理，再叠加物理表现。
- [x] 获准后运行 `CityCourier.PhysicalReaction`、相关 CharacterActions 套件，检查不同方向及连续受撞的画面、角色位置和恢复状态。
- [x] 随共享动画图和组件接口统一提交；各阶段测试证据独立保留（见实施取舍）。

### Task 8: 玩家入口、验证场景和最终交付

**Files:** 创建 `Content/Python/build_epic_character_validation.py`、`validate_epic_character_integration.py`、`Tests/EpicCharacterIntegrationTests.cpp`、`docs/epic-character-integration.md`；更新实际生成资产、来源清单和本计划勾选状态。

**Interfaces:** `/Game/CityCourier/Animation/Epic/DA_CourierEpicAnimation` 为真实角色配置；`/Game/CityCourier/Maps/L_EpicCharacterValidation` 提供走跑区、翻越障碍、抓取物、按钮、台阶与受撞目标。`validate_integration(output_path: str) -> dict` 区分 assets/code/build/runtime 四类完成状态。

- [x] 将配置连到真实 `BP_CourierCharacter`，动画资产校验完成前保持兼容配置。验证场景沿用游戏角色和交互入口，不能用示例角色替代测试。
- [x] `CityCourier.EpicCharacterIntegration.RealPlayerFlow` 验证同一骑手完成移动、翻越、抓放、按钮、受撞和骑车切换；生成脚本二次运行不重复放置演示对象或覆盖人工修改。
- [x] 运行 Python AST、`git diff --check`、资产闭包/Skeleton/数据库/PhysicsAsset 检查，并形成缺项报告。
- [x] 如尚未获得构建授权，此时提供具体代码与资产变更、说明新原生类型依赖，并请求本轮编辑器构建及相应运行验证授权；不自行启动构建或游戏，不声称全部集成完成。
- [x] 获准后顺序执行 `./scripts/Build.ps1 -Target Editor`，再按任务过滤运行 `./scripts/Test.ps1`。最后执行现有 `CityCourier.CharacterActions`、`CityCourier.Interaction`、`CityCourier.Scooter`、`CityCourier.VehicleTraversal` 回归；运行过滤前核对实际层级名称，报告所选用例和数量。
- [x] 按不启动游戏窗口的授权，使用自动化游戏 World 和离屏 SceneCapture 保存最终姿态及接触/取消/恢复证据；未运行交互式 PIE。
- [x] 随共享动画图和组件接口统一提交；各阶段测试证据独立保留（见实施取舍）。

## 计划自审

- 三个示例的来源、依赖、不同骨架与现有动画代理冲突分别由 Task 1–3 处理。
- 翻越的通行和移动基座约束由 Task 4 处理；抓取/按钮、目标点与 Rig 分别由 Task 5–6 处理；物理模拟恢复由 Task 7 处理。
- Review Focus 的五类失败条件均有明确测试归属。
- 现有动作生产测试需要随“旧跳跃蒙太奇由新图接管”调整，但保留攻击接触、姿态和骑车回归；不删除测试来掩盖行为变化。
- 无构建/运行授权时保持相关检查未执行；需要新原生类的生成步骤在编译前不能计为完成。
- 执行方式已由用户确认：在当前会话顺序实施，因为动画图、交互参数和取消状态共享同一角色，前后任务依赖紧密。
