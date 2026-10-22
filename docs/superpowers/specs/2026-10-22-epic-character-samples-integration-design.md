# Epic 角色动作示例接入设计

日期：2026-10-22

状态：本轮已获准构建、资源生成和自动化测试；正式角色已接入，56 项相关自动化及全部资源审计通过，独立最终审查待完成。未打包、未启动游戏窗口。详细实施与证据见 docs/epic-character-integration.md。

## 已授权的目标与约束

用户要求参考三个 Epic 官方示例接入 CityCourier：GASP 改善走跑、跳跃和翻越；Stack O Bot 提供拿取、按按钮以及手脚对位的参考；Content Examples 提供受撞、物理反应和动画技术的参考。

目标工程是 UE 5.6.1 的单机配送游戏。沿用骑手外观、订单系统、骑车和既有动作输入。资源使用按需迁移，不把示例的机器人、测试城市、GameMode 或 PlayerController 直接替换进游戏。

依照仓库 AGENTS.md，本次编辑不自动执行 C++ 构建、自动化测试、打包或启动游戏。迁移和生成 uasset 所需的编辑器资产处理与游戏启动分开；记录实际执行的资产检查，玩法和画面验证在获得对应运行授权后进行。不能把静态检查或脚本完成当成玩法已验证。

## 本机资源核实

| 工程 | 路径 | EngineAssociation | 文件数量 | 文件大小（GiB） |
| --- | --- | --- | ---: | ---: |
| Game Animation Sample | `D:/UnrealProjects/GameAnimationSample` | 5.6 | 2230 | 5.00 |
| Stack O Bot | `D:/UnrealProjects/StackOBot` | 5.6 | 1957 | 1.69 |
| Content Examples | `D:/UnrealProjects/ContentExamples` | 5.6 | 3439 | 10.78 |

以上是下载后递归统计的文件大小，不是下载流量，不包含之后运行编辑器产生的额外缓存。

已定位的参考资产：

- GASP：`/Game/Blueprints/ABP_SandboxCharacter`、`CBP_SandboxCharacter`、`ABP_GenericRetarget`、UEFN Mannequin 的 IK Rig、移动动画、Traversal/Mantle 与 Traversal/Vault 动画及蒙太奇。
- Stack O Bot：`/Game/StackOBot/Blueprints/Character/BP_Bot`、`GameElements/BPC_InteractionHandler`、`PhysicsElements/BP_Crate`、`Characters/Bot/Rig/CR_Bot_Correction`。资产名称表包含 `Grab_Check`、`Grab_Init`、`Grab_Update`、`Grab_Clear`、`PhysicsHandle`、`InteractionWorldLocation` 和手脚 IK 相关字段。
- Content Examples：`/Game/ExampleContent/Physics/Blueprints/PhysicsAnimationBlendingHittReaction`、`PhysicsAnimationBlendingPhysicalAnimationComponent`，以及 `Maps/Animation/PhysicalAnimation`。资产名称表包含局部物理模拟、冲量、物理混合权重和 PhysicalAnimation 设置调用。

资产定位及名称表检查只能确认资源和相关标识存在，尚不构成蓝图执行流程或迁移后兼容性的验证。实现前通过编辑器资产接口核对类型、实际依赖和参考骨架。

## 当前项目的接入障碍

1. `UCourierAnimInstance` 使用 `FCourierAnimProxy::Evaluate` 直接输出姿态，并在 C++ 中手动计算动作 Slot。这个流程绕过生成的 AnimGraph，直接增加 Motion Matching 或 Control Rig 节点不会影响最终姿态。
2. 当前骑手采用自有骨架。已有重定向工具明确处理 `shin` 与 Epic `calf`、`spine/chest` 与 Epic 躯干链的区别，也处理导入比例。不能只根据骨骼同名指定兼容 Skeleton。
3. `UCourierActionComponent` 管理攻击、受击和跳跃蒙太奇；动作时序控制命中窗口。新动画不能接管或重复触发这些玩法事件。
4. 现有 `ICourierInteractable` 和 `ACourierCharacter` 已有距离、遮挡、优先级及状态检查。抓取、按钮和配送交互应使用同一个选择入口，避免多个 E 键处理器竞争。
5. 汽车攀跳已有引擎盖、车顶碰撞和随车移动规则。新增翻越不应破坏这些移动基座行为。

## 方案比较与选择

### 推荐：保留角色玩法，迁移动画与独立功能

为当前角色建立动画适配层，按需重定向 GASP 动画和配置；按 Stack O Bot 的组件及目标点思路实现人形交互；按 Content Examples 的方式叠加局部物理反应。所需依赖进入当前工程，来源工程保持独立。

这需要修改动画输出链，但能保持配送、骑车、攻击和存档等既有系统的职责。

### 备选：直接换成示例角色

用 GASP 的角色蓝图代替当前骑手，再移植现有玩法。可以较快运行原始移动演示，但会扩大角色、输入、镜头、骨架及动作系统的迁移范围，不作为本次方案。

### 备选：只替换动画片段

继续使用当前固定片段播放器，仅替换走跑和跳跃动画。改动较小，但不能完整实现所选的运动匹配、交互 IK 和翻越目标对齐，因此不能作为三个示例已经接入的交付结果。

## 动画输出与资源迁移

### 统一动画入口

由 `UCourierAnimInstance` 向标准 AnimGraph 提供移动、空中、姿态、骑车、动作、交互目标等状态。迁移后的图依次处理移动姿态、既有动作 Slot、交互与脚部对位；局部物理反应由 SkeletalMesh 的物理混合流程处理。

在新图和依赖检查通过前保留旧动画路径作为明确的兼容配置；资源不完整时报告原因，不静默宣称启用新系统。避免新旧代理同时更新蒙太奇或重复消耗动作时钟。

当前 `CourierArtBuilder` 的重建操作会清空 AnimGraph。新图采用独立生成入口；旧生成器不能覆盖已配置的 Epic 动画图。

### GASP

- 保持 CharacterMovement 作为当前移动模型，本次不改为 Mover。
- 核对源动画参考姿态、朝向、单位和 Root Motion；建立 UEFN 到 Courier 的 IK Rig 与 Retargeter，输出使用 Courier Skeleton 的动画。
- 按迁移后的骨架建立 Pose Search Schema、数据库和动作选择配置，覆盖站立、行走、跑步、起停、转向、跳跃、下落和落地。数据库重新建立索引，不能照搬引用源骨架的缓存。
- 保留当前蹲伏、趴下、骑车及攻击蒙太奇入口，用明确的状态优先级与移动姿态组合。
- 翻越使用障碍高度、深度、落点和胶囊通行检查。将选定的 Vault/Mantle 动画与 Motion Warping 目标结合；目标失效或出口受阻时安全取消并恢复移动。
- 从空格输入尝试可用的翻越，条件不满足时沿用普通跳跃。对移动汽车沿用现有攀跳逻辑，首版不在移动汽车上执行固定世界目标的翻越动画。

## Stack O Bot 交互适配

### 物理拿取

- 新增可复用的可拿取物体组件或 Actor，通过 `ICourierInteractable` 参与当前目标选择。
- 对显式配置为可抓取、允许模拟物理的对象使用 PhysicsHandle；检查距离、遮挡、质量限制和状态，不允许抓取建筑、车辆或任意场景网格。
- E 拿取，再次 E 放下。持有时提供稳定的目标点、旋转和双手目标；释放时恢复抓取前状态。目标销毁、进入骑车、受击或界面抑制时统一释放。
- 此功能不把订单餐品自动改成物理库存，也不新增物品栏。先在动作验证场景提供一个可拿取物体，配送逻辑继续以现有订单状态为准。

### 按钮与手脚对位

- 新增可配置的交互目标点，支持指定使用哪只手、触碰位置及接触时机。对位逻辑参考 `CR_Bot_Correction`，使用 Courier 骨架重新建立，不能直接使用机器人 Rig。
- 按钮在有效接触时刻只提交一次操作，并重新核对目标和交互条件。取消或失败不得提前触发功能。
- 首版在验证场景提供一个可操作按钮；现有取餐、开门等目标通过可选目标点逐个适配，不强制全部播放同一个按钮动作。
- 脚部 IK 只在适合的站立/地面状态求解。空中、翻越、骑车和趴下使用各自的状态权重，避免脚被地面锁住。

## Content Examples 物理受撞适配

- 为 Courier Skeleton 核对或生成合适的 PhysicsAsset，检查身体比例、约束、碰撞和自身胶囊的关系。
- 使用 PhysicalAnimationComponent 和局部模拟/混合，为命中附近的上身或肢体施加受限冲量；胶囊和移动模型仍负责角色位置。
- 根骨骼与整体移动不进入全身布娃娃模拟。首版不增加死亡、布娃娃起身或失控摔倒系统。
- 受撞权重平滑进入和退出，恢复先前模拟、碰撞及运动设置。重复冲量设上限，骑车、销毁、取消时必须清理。
- 既有攻击命中时序仍由动作组件负责；物理受撞是表现叠加，不增加第二次伤害或改变攻击判定。

## 状态优先级与取消

状态冲突按以下原则协调：骑车或输入抑制会结束普通交互；受击会取消抓取/按按钮/翻越的可中断阶段；攻击和交互不同时占用全身动作。恢复路径必须涵盖正常结束、目标销毁、控制权变化、状态中断和组件 EndPlay。

每个组件保存它改动前的设置，按所有权恢复。不能通过写死“恢复 Walking”掩盖角色本来处于空中、骑车或界面抑制的情况。

## 资源布局、依赖和来源

- 按实际 Asset Registry 依赖生成迁移清单，保留来源资产路径与新包路径的映射；不要用文件系统搬动 uasset 后假设内部引用自动更新。
- 在 `Content/CityCourier/Animation/Epic/` 下组织经过适配的移动、翻越、Rig 和配置。独立使用的源依赖按需要保留包路径，避免与当前内容重名。
- 编辑器迁移/生成脚本放入 `Content/Python/`，过程报告写入忽略的 `Saved/Validation/`。脚本应可重复运行，遇到缺源、缺插件、骨架不匹配或已有人工修改时明确失败。
- 在 `docs/art/asset-manifest.csv` 记录实际引入的 Epic 资源、来源 Fab URL、用途及原始许可说明；不把这些资源标成 CC0，也不混入既有自制资源来源。
- 插件按实际需要启用 PoseSearch、Chooser、MotionWarping、AnimationWarping、IKRig 和 ControlRig 等，并核对 UE 5.6 模块依赖。PhysicalAnimation 的组件属于 Engine，不能凭名称新增不存在的独立插件。

## 主要修改位置

| 范围 | 预计位置 |
| --- | --- |
| 动画状态、输出链、构图入口 | `Player/CourierAnimInstance.*`、`CourierArtBuilder.*`、独立 Epic 动画生成器 |
| 翻越状态及动作冲突 | `Player/CourierCharacter.*`、新增 Traversal 组件、`CourierActionComponent.*` |
| 抓取、按钮、目标点 | `Player/CourierInteractable.h` 的兼容扩展、独立交互组件及演示 Actor |
| 局部物理反应 | 独立物理反应组件、角色命中入口和 PhysicsAsset |
| 模块和资源依赖 | `CityCourier.Build.cs`、`CityCourier.uproject`、动画与验证场景资源 |
| 迁移与来源记录 | `Content/Python/`、`docs/art/asset-manifest.csv` |

不修改现有未跟踪的 storefront 脚本、文档或快捷方式。

## 验收与验证

### 不启动游戏即可执行的检查

1. 三个源工程的版本、资源类型及依赖报告可追溯。
2. 迁移资产引用完整，所有输出动画绑定 Courier Skeleton，Root Motion 策略与用途一致。
3. Pose Search 数据库、Chooser、Retargeter、动画图及 PhysicsAsset 完成编辑器资产检查；缺少依赖必须阻止启用新路径。
4. 检查 diff、脚本语法及来源清单。未运行 C++ 构建、游戏测试或画面检查时在交付中明确注明。

### 获得构建/运行授权后执行

1. 先构建再运行新增的 EpicAnimation、Traversal、Interaction 与 PhysicalReaction 自动化用例，并回归 CharacterActions、Scooter 和 VehicleTraversal 相关套件，所有 UE 进程顺序执行。
2. 在 PIE 中录制走跑起停、转向、跳跃落地、不同高度障碍、目标失效及出口受阻的姿态与状态证据。
3. 验证抓取/释放、按钮触碰只触发一次、目标销毁、遮挡和距离失败；检查手脚贴合与状态权重。
4. 验证不同方向受撞、连续受撞、局部物理恢复及骑车状态切换；角色不得穿地、飞起或失去控制。
5. 验证取餐、送达、开门、手机/地图/暂停和电动车骑乘仍可用。仅有源文件或新组件不能算作玩家可用的集成。

## 交付边界

交付应包含实际依赖资产、适配代码、可重复的迁移/生成脚本、来源记录以及执行过的检查结果。未构建或未运行的功能只能标为待验证；尚未完成某一示例的适配时，不能声称三个系统均已接入。
