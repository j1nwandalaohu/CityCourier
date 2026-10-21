# Citizen Behavior Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task after user review and execution-method selection. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成具有性格差异、真实反击、有限追击与搜索、逃跑、目击援助和脱战恢复的街头行人行为。

**Architecture:** 保留 `AAmbientPedestrian`、独立 CharMorph 骨架和 12 人对象池；将行为决策、导航移动、攻击时间线分别实现为独立单元。身体反应优先于行为，管理器限制路径查询和围攻，所有目标使用弱引用。

**Tech Stack:** Unreal Engine 5.6.1 / Windows / C++、NavigationSystem、现有 Native AnimInstance、Blender 4.5、UE Python 动画导入。

**Spec:** `docs/superpowers/specs/2026-10-21-citizen-behavior-design.md`（用户已确认；执行前同时阅读本文件和设计）。

## Global Constraints

- 不自动编译、打包或启动游戏；之前的编译运行请求不授权本次执行这些操作。
- 不自动添加或运行测试。下面的验收项和命令是待用户要求验证时执行的清单，不是已完成的测试或编译证据。
- 按需生成和导入本功能动画；需要新 C++ 编辑器接口的导航资源生成须等待用户明确要求编译，不能偷偷触发编译来完成导入。
- 保持 12 人池、现有五个身体反应、单重拳或 1.2 秒内双轻拳击倒、倒地 2 秒和受阻起身规则。
- 不改变骑手骨架、既有动画、玩家存档或订单经济规则；反击通过现有受击动作/打断入口反馈。
- 参数采用设计中的初始值，不标注为 GTA 原版值；实现原创 UE 逻辑，不引入 GTA 资源或直接移植其源码。
- 改动文件显式暂存，提交说明使用中文；已有 storefront 脚本、说明和快捷方式不纳入本功能。
- 按任务顺序串行实施；是否使用子代理取决于用户选择，不提前派发。UE 编辑器资源操作也按顺序执行。

## Review Focus

1. **复用和延迟路径结果**：Actor 回收后重新激活，上一轮路径、威胁或攻击令牌不能应用到新一轮（任务 1、3、6）。
2. **目标在遮挡后移动**：搜索不得读取目标当前位置作弊，目标销毁、骑车离开或进入室内要有退出路径（任务 2、3）。
3. **低帧跨越命中窗**：一次 Tick 跨越整个有效窗口仍按拳轨迹处理至多一次接触；被打断后不得补发伤害（任务 4、5）。
4. **无 NavMesh、堵路和无地面**：没有数据时不穿墙、不沿失效路径走、不用传送恢复（任务 3）。
5. **多名行人和界面切换**：援助事件不递归传播，攻击目标受抑制时不生成幽灵接触，围攻令牌在所有退出路径释放（任务 4、6）。

以上每项均列入所属任务的延后验收清单。

## File Structure

| 文件 | 职责 |
| --- | --- |
| `Player/CourierStrikeReceiver.h` | 带攻击者的命中上下文及兼容入口 |
| `Player/CourierActionComponent.cpp` | 玩家拳击传递真实攻击者 |
| `Ambient/CitizenBehavior.h/.cpp`（新增） | 配置、纯决策状态和退出原因 |
| `Ambient/CitizenNavigation.h/.cpp`（新增） | 路径状态、限速地面移动和局部避障 |
| `Ambient/CitizenCombat.h/.cpp`（新增） | 行人攻击时间线、实例 ID 和轨迹接触 |
| `Ambient/AmbientPedestrian.h/.cpp` | 上下文接收、感知快照、身体状态与各单元协调 |
| `Ambient/AmbientTrafficManager.h/.cpp` | 个体初始化、路径队列、围攻和目击 |
| `Ambient/CitizenAnimInstance.h/.cpp` | 十段行人动画和状态混合 |
| `Player/CourierArtBuilder.h/.cpp`、`CityCourier.Build.cs` | 编辑器导航构建接口和 NavigationSystem 依赖 |
| `Content/Python/build_citizen_navigation.py`（新增） | 在指定地图增量生成、保存室外静态导航 |
| `tools/art/build_citizen_behavior.py`（新增） | 生成五段新行为动画，保护原五段 |
| `Content/Python/import_citizen_behavior.py`（新增） | 导入独立行人骨架的行为动画 |
| `AssetsSource/CityCourier/Ambient/CharMorph/Behavior/`（新增） | Blender、FBX、来源和姿态清单 |
| `Content/CityCourier/Art/Ambient/CharMorph/Behavior/`（新增） | 五段新 UE 动画资源 |
| `docs/art/citizen-behavior.md`（新增）等说明 | 编辑、行为参数、验收状态和资产来源 |

表中 C++ 相对路径均位于 `Source/CityCourier/`。导航生成只处理指定地图及本脚本拥有的导航标记，不重跑整张城市/美术地图生成器。

### Task 1: 命中上下文与兼容接收

**Files:** 修改 `Player/CourierStrikeReceiver.h`、`Player/CourierActionComponent.cpp`、`Ambient/AmbientVehicle.h`、`Ambient/AmbientPedestrian.h/.cpp`。

**Interfaces:** 新增普通 C++ 结构 `FCourierStrikeContext { FGuid InstanceId; TWeakObjectPtr<AActor> Instigator; FVector Origin; ECourierAction Action; }`。接口新增 `virtual bool ReceiveCourierStrikeContext(const FCourierStrikeContext& Context)`，默认转发现有 `ReceiveCourierStrike(FGuid,const FVector&,ECourierAction)`；保留旧虚函数签名，训练靶和骑手无需立刻新增覆盖。

- [x] 将 `TraceSlice` 的接收调用改为上下文入口，`Instigator` 明确为本次真实出拳的 Rider；保留阻挡、实例去重和中断代次检查。
- [x] 行人覆盖新入口；先通过原有距离、动作、重复和身体状态检查，仅成功受击后记录攻击者弱引用和事件信息。旧入口继续只触发身体反应，绝不查找第一个玩家补目标。
- [x] 增加行人 `uint32 ActivationGeneration`，每次退出人口激活和重新分配巡逻生命周期时递增，供后续异步/队列请求识别旧结果。将基类 `SetPatrol(const TArray<FVector>&)` 声明为 virtual，行人覆盖时先清理旧行为再调用 Super，避免管理器通过基类指针重新分配路线时漏掉清理。保存原有受击者保护条件。
- [x] 静态审阅所有 `ReceiveCourierStrike` 调用点，确保旧入口仍可用、上下文不得来自全局玩家猜测，重复输入不更新敌意。

**延后验收：** 真实拳击记录攻击者；旧位置型调用不建立目标；无效/自伤/已销毁攻击者不建立追击；重复 ID 不增加压力；旧训练靶仍可被击中。

### Task 2: 可配置性格、决策和身体状态衔接

**Files:** 新增 `Ambient/CitizenBehavior.h/.cpp`，修改 `Ambient/AmbientPedestrian.h/.cpp`。

**Interfaces:** 定义 `ECitizenPersonality { Cautious, Ordinary, Aggressive }`、设计中的十个 `ECitizenBehaviorState`、`ECitizenExitReason { None, TargetInvalid, Timeout, LeashExceeded, LostTarget, Unreachable, Safe, Recycled }`；`FCitizenBehaviorConfig::ForPersonality(ECitizenPersonality)` 返回完整默认配置。

`FCitizenPerception` 包含 TargetValid、TargetVisible、TargetMounted、BodyAvailable、AttackReady、PathFailed、MoveProgress、TargetDistanceCm、TargetAngleDeg、VisibleTargetPosition、SelfPosition。不可见时快照不携带实时目标坐标。

纯规则单元 `FCitizenBehavior` 提供 `Reset(int32 Seed,const FCitizenBehaviorConfig&)`、`RegisterThreat(FGuid EventId,FVector Origin,bool bHeavy,bool bDirect)`、`Advance(float Delta,const FCitizenPerception&)`，暴露只读的 State、ExitReason、Pressure、LastSeenPosition、IncidentAge、StateAge 和本次 Decision。Actor 保存真实目标弱引用，规则单元不拥有 Actor。

- [x] 定义普通配置：240/380 cm/s、135/180 cm 近战滞回、±35°、记忆 3 秒、搜索 5 秒、追击 20 秒、事件半径 3500 cm；好斗型为 30 秒/5000 cm。
- [x] 实现性格固定种子响应、0.35–0.75 秒评估时间、轻拳压力 +20/重拳或击倒 +45、普通 70/好斗 100 撤退阈值、安全时每秒恢复 5；同一重拳击倒只累计一次压力。
- [x] 事件时间从首个有效威胁开始累积，不因新命中重置；评估动画时钟则在身体可用后开始。身体不可用时不输出主动移动或攻击决定，恢复后按保留敌意重新决策。
- [x] 在行人 Tick 中按 0.25 秒感知节奏驱动规则，实际身体/动画时钟继续逐帧处理；保留大 Delta 跨状态推进，拒绝 NaN、负值和无效目标。
- [x] 定义重复碰撞警告：同一对象 2 秒内三次有效阻挡接触触发 Confront 最多 1 秒，只朝向警告，未受击者不能因此获得攻击权；接触记忆有界并在回收时清理。
- [x] 检查身体恢复不再无条件销毁行为，但旧的无上下文反应仍可恢复巡逻。目标不可见时只更新记忆计时，禁止更新 LastSeenPosition。

**延后验收：** 固定种子结果稳定；三类性格不同；重复/连续命中不能无限刷新预算；倒地恢复后应战或撤退；普通碰撞不会开战；遮挡后记忆位置不变；大 Delta/暂停不产生额外决策。

### Task 3: 安全导航、追击、逃跑和巡逻恢复

**Files:** 新增 `Ambient/CitizenNavigation.h/.cpp`、`Content/Python/build_citizen_navigation.py`；修改行人、交通管理器、`Player/CourierArtBuilder.h/.cpp`、`CityCourier.Build.cs`；生成时只更新明确指定的 `L_CourierCity` / `L_CourierArtPreview` 导航数据。

**Interfaces:** `FCitizenPathRequest` 保存行人弱引用、ActivationGeneration、RequestSerial、Goal；`FCitizenPathResult` 保存同样标识、完整路径 Points 和 `ECitizenPathStatus { Complete, Partial, Unreachable, NoNavigation }`。管理器提供 `QueueCitizenPath(const FCitizenPathRequest&)` 和 `ProcessOneCitizenPath()`；行人提供 `ApplyCitizenPath(const FCitizenPathResult&)`。

`FCitizenNavigation` 提供 `Reset()`、`SetPath(const TArray<FVector>&)`、`Advance(AAmbientPedestrian&,FVector Goal,float Speed,float Delta)`；Advance 返回实际位移与受阻原因。`UCourierArtBuilder::BuildCitizenNavigation(UWorld* World)` 是 editor-only 的 Python 可调用入口。

- [x] 增加 `NavigationSystem` 运行依赖，编辑器部分沿用 `UnrealEd`。实现室外静态 NavMesh 构建帮助函数：导航代理半径 40 cm、高 180 cm；刷体积只覆盖道路/人行道条带、地面上下必要高度，排除室内和屋顶；使用固定标签增量更新。
- [ ] Python 脚本先检查地图和新 native 方法是否存在；方法尚未编译时清晰退出、不调用构建脚本。接口可用后生成并保存静态导航，不启用运行时动态全城重建，不删除无关地图 Actor。
- [x] 实现去重路径队列，每帧最多一项；将处理入口放在管理器人口刷新 early-return 之前。按半径 40 cm/高 180 cm 选择匹配导航数据，拒绝部分路径作为已到达目标的依据。
- [x] 0.75 秒内不重复规划，除非路径失效；目标可见且移动超过 200 cm 后允许更新目的地。不可见时目的地只能是 LastSeenPosition。收到结果时同时核对弱引用、激活代次和请求序号。
- [x] 实现独立的行人移动循环：有界子步、扫掠、旋转限速、复用 `FindStreetGround` 的支持高度规则、台阶/坡度与下一落脚点检查。行人巡逻也使用这条循环，避免调用车辆逐帧全世界乘客枚举。
- [x] 直线通路通过整段碰撞和地面采样后才可直走；局部绕障尝试侧向安全路径，失败则排队完整路径。缺少 NavMesh 时只允许已经确认安全的短段，不假设直线可达。
- [x] 2.5 秒无进展触发重规划，最多两次再退出。路径请求队列等待时间不算已完成移动，不瞬移、不忽略墙/门/车辆。
- [x] 逃跑每 0.5 秒评估背离威胁的 8 个候选方向，选择可达、可落脚且能增大距离的目标；无解进入 Cower 重试。20 米外连续 4 秒无可见威胁才安全退出。
- [x] ReturnToPatrol 先导航至原巡逻线的最近可达接续点，再切 Patrol；无法返回时原地安全等待并限频重试，不从墙内直接连到旧终点。

**延后验收：** 无 NavMesh 不穿墙；绕路和关闭门阻挡；无地面/车顶拒绝；目标进室内、骑车离开及销毁可脱战；搜索不作弊；堵路退出；旧代次/旧请求结果拒绝；巡逻返回不瞬移；每帧路径查询不超过一项。

### Task 4: 行人攻击时间线和真实接触

**Files:** 新增 `Ambient/CitizenCombat.h/.cpp`，修改行人、`Player/CourierActionComponent.cpp`（仅命中记录清理和现有反馈衔接）。

**Interfaces:** `ECitizenAttackPhase { None, Windup, Contact, Recovery }`；`FCitizenAttackSlice { FGuid InstanceId; bool bLeft; float Start; float End; }`；`FCitizenCombat::TryStart(bool bLeft,float FacingYaw)`、`Advance(float Delta)`、`Cancel()`、`Reset()`，Advance 返回本帧与命中窗交集的切片。行人负责世界接触，动画读同一 InstanceId、Elapsed 和锁定朝向。

- [x] 实现左右拳独立实例和锁定朝向，初始时长 0.8 秒、接触窗 [0.22,0.38] 秒、间隔由固定种子在 1.0–1.5 秒选择；开始时实际距离不超过 135 cm、角度不超过 35°，且身体可用、目标可接受动作、专用动画可播放。
- [x] 收到切片后以不大于 1/120 秒间隔，复用 `FCourierActionAnimation::SampleFist` 采样行人手部；12 cm 球扫掠，检查从身体到接触点的遮挡与阻挡先后，仅对锁定目标接收一次。
- [x] 新上下文接收入口向目标传递本行人；距离只用于开始门槛和最大轨迹约束，不替代接触。玩家蹲姿/趴姿/腾空位置以实际碰撞为准；骑乘、输入抑制、目标销毁时取消这次接触。
- [x] 行人身体反应、失去攻击令牌、动画缺失或骨架不兼容时递增中断代次并取消，防止同一 Tick 后续切片继续命中。落空也完成收招和冷却。
- [x] 玩家仅在现有 `ReceiveStrike` 成功后显示“遭到反击”，不额外扣钱/餐品/生命。将 Received 去重记录改为带时间戳的容器，保留最近 10 秒、最多 256 项，EndPlay 清空，避免长时间反击导致记录无限增长。

**延后验收：** 正面命中、走开落空、背后转向限制、墙/门/车阻挡、跨命中窗的大 Delta、受击打断、缺动画、界面和骑乘保护；一次实例不会重复命中，去重内存有界。

### Task 5: 专用动画、可编辑来源和动画桥接

**Files:** 新增行为动画生成/导入脚本、Behavior 源文件夹和五段 UE 动画；修改 `Ambient/CitizenAnimInstance.h/.cpp`、行人动画就绪查询、`docs/art/asset-manifest.csv`。

**Interfaces:** `ECitizenClip { Idle, Walk, Hit, Fall, GetUp, Guard, PunchL, PunchR, Run, Cower, Count }`；`UCitizenAnimInstance::HasCombatClips() const` 校验攻击、警戒和骨架；保持现有五段索引 0–4，新增 5–9。

- [x] 从已保留的 Mesh2Motion GLB 读取动作目录，记录实际选用的动画名和上游哈希；使用独立生成器复用旧脚本的重定向和贴地方法，旧五段 FBX、源 Blend 和 UE 动画不重导入。
- [x] 生成 Guard、PunchL、PunchR、Run、Cower 五段、60 fps、原地根骨、独立行人骨架的 Blend/FBX；出拳时长 0.8 秒并使最大伸展落在 [0.22,0.38]。不能使用骑手 skeleton 替代。
- [x] manifest 记录来源、许可、采样、哈希、拳轨迹及根骨/脚底约束；导入脚本保护原骨架、网格、五段反应与所有骑手资源的哈希，只保存新 Behavior 动画。
- [x] 在无需编译新 C++ 的前提下执行 Blender 生成与 UE 动画导入；若依赖或 native 接口尚未可用，记录准确原因，保留源文件和可执行脚本，不伪造已生成的 uasset。
- [x] 将 AnimProxy 固定数组和遍历上限改为 Count；身体反应优先，其次攻击，再按行为选择 Guard/Run/Cower/Walk/Idle。循环只用于待机、移动、警戒、防护，攻击按时间线采样并收招。
- [x] 同一左右拳动画再次播放时用攻击 InstanceId 重置采样/过渡，而非只比较 AnimationIndex；取消攻击后不能继续保持旧拳姿。
- [x] 补齐资产来源清单与编辑说明；已有 `PedestrianCombatTests.cpp` 中“五段”断言需要与新 Count 一致，但不在当前任务中添加或运行新测试。

**延后验收：** 十段全部存在、独立骨架匹配、无玩家资产变更；压缩前后姿态正确；贴地和倒地起身维持原样；同一拳连续播放确实重启；可见攻击和有效窗口一致。

### Task 6: 有限援助、围攻、人口复用与调试

**Files:** 修改交通管理器、行人、行为配置；新增 `Ambient/CitizenCrowdCoordination.h/.cpp` 分离目击去重与攻击资格计算，避免管理器文件同时承载所有规则。

**Interfaces:** 管理器提供 `NotifyCitizenAssault(AAmbientPedestrian& Victim,const FCourierStrikeContext&)`、`TryAcquireCitizenAttack(AAmbientPedestrian&,AActor& Target)`、`ReleaseCitizenAttack(AAmbientPedestrian&)`。`FCitizenWitnessEvent` 保存源 EventId、攻击者/受害者弱引用、Origin 和 HopCount；只接受 HopCount=0 的原始受击事件进行分发。

- [x] 每次激活按 Slot+ActivationGeneration 生成随机流，30/50/20 权重分配性格和错开的感知相位；固定种子下分配稳定。
- [x] 对一次成功受击检查最多 12 个池内行人，按距离排序选 12 米内、有视线的最多 4 位目击者。自身和攻击者排除；已有直接威胁者不被覆盖；事件 ID 去重至少保留整个事件生命周期并设置容量上限。
- [x] 目击谨慎者逃离，愿意援助者最多两人获得近战参与资格，其他人保持间距警戒；源事件传递 HopCount=1 后禁止再次分发，普通逃跑和同伴接收事件不触发递归战斗通知。
- [x] 每个目标只允许一个有效出拳持有者，按等待顺序轮换；持有者必须仍活跃、在场且处于合法攻击状态。超时兜底为 1.2 秒，收招/打断/脱战/回收/EndPlay 立即释放；尚在收招者不抢回令牌。
- [x] 统一 `ResetCitizenRuntime()` 清理目标、决策、压力、路径队列代次、目击缓存、接触窗口、攻击资格和速度，恢复碰撞及网格偏移；回收保留现有 180 米边界。
- [x] 增加只在开发构建可用的诊断开关 `Courier.CitizenDebug`，显示身体/行为、性格、目标、最后可见点、预算、路径状态、令牌和退出原因；关闭时不逐帧输出日志。

**延后验收：** 四名目击者上限、两名参与者/一人出拳限制；目击不覆盖直接目标、不递归传播；持有者倒地/销毁/复用立即释放；骑手打开界面时无幽灵拳；长时间事件缓存有界。

### Task 7: 集成审阅、交付说明与待授权验收

**Files:** 新增 `docs/art/citizen-behavior.md`；更新 `docs/art/citizen-reactions.md`、`README.md`、`docs/playing.txt`、本计划完成勾选；仅在确实生成时更新地图/动画资产与来源清单。

- [x] 沿“玩家真实命中 → 受伤恢复 → 应战或逃跑 → 追击/搜索 → 接触反击 → 脱战 → 巡逻/回收”逐条核对接口、身体优先级、销毁和复用路径。
- [x] 说明全部性格/时限/距离参数、调试入口、动画制作和导入方式；将历史验收数据明确标为旧身体反应版本，不用来证明新增 AI 已通过。
- [x] 执行文本和 Git 静态审阅：逐个读取差异、确认只含本功能、`git diff --check`；检查上下文接口默认适配、AnimProxy Count、编辑器宏、NavigationSystem 依赖和保护资源哈希。
- [x] 报告源码、源资产、UE 动画、导航数据分别完成到哪一步，以及哪些仍等待用户要求编译后才能生成；禁止用“源码已修改”代替“游戏行为已验证”。
- [ ] 若按既定流程提交，显式暂存本功能文件、使用中文说明；不自动推送，不把无关 storefront 文件和快捷方式加入提交。

**用户明确要求验证后才执行的顺序：**

1. `./scripts/Build.ps1 -Target Editor -EngineRoot D:/sdk/UE_5.6`。
2. 执行 `build_citizen_navigation.py` 生成/保存地图导航；如动画导入仍待执行则先完成导入。UE 操作串行，不自动启动 PIE。
3. 用户要求测试时扩充相应行为、导航、接触、群体和回收用例，运行 `CityCourier.Ambient`、`CityCourier.CharacterActions`，并记录本计划各验收清单的实际结果。
4. 用户要求运行时做 PIE 可见反击、绕障、丢失视野、逃跑和多行人状态录制；用户要求打包时再执行 Shipping 打包和启动。
5. 用户要求性能验证时使用相同人数、路线、画质、实际分辨率与隔离 UserDir 对照；不把编辑器或短时静止数据标成整条路线验收。

## Plan Review

- 设计覆盖：上下文与性格（1–2）、身体恢复（2）、导航/逃跑/返回（3）、真实反击与玩家反馈（4–5）、目击/围攻/回收/性能（3、6）、说明与验收状态（7）。
- 接口一致：上下文入口保留旧适配；导航代次及序号贯穿排队和接收；动画 Count 与同拳重播使用同一攻击实例。
- 执行约束：后续修改不自动编译、测试、打包或启动，遵守用户当前任务授权。生成/导入行人行为动画不依赖改动骑手骨架。
- 当前状态：源码、五段源动画/UE 动画和静态审阅已完成。2026-10-21 按用户“编译一下运行”要求，完成编辑器编译、两张地图导航生成、Shipping 打包和启动；构建记录见 `docs/art/citizen-behavior.md`。未运行自动化测试或行为实玩验收。用户随后要求提交并推送本次改动。
