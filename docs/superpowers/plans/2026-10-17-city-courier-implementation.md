# CityCourier Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 交付一款 Windows 单人 3D 中国城市外卖游戏，能步行、骑行、接单、取餐、送达、结算、升级车辆并保存进度。

**Architecture:** UE5 C++ 实现订单规则、玩家资料、移动与导航；Blueprint 和 UMG 配置场景资产、动画与界面。订单与经济规则可独立验证，场景交互必须通过同一套规则接口，最终以实际 Windows 打包运行验收。编辑器脚本负责创建和维护资源，运行时不依赖 Python 或编辑器模块。

**Tech Stack:** UE 5.6、Windows x64、C++、Blueprint、Enhanced Input、UMG、UE Automation Framework；PowerShell 构建入口；Python 仅用于编辑器资产制作；Blender 用于原创模型和动画制作。

**Spec:** [已获用户确认的首版设计](../specs/2026-10-17-city-courier-design.md)。执行者先读设计，再读本计划；本计划不扩大已确认范围。

**Status:** 用户于2026-10-17回复“1”，确认计划并选择主代理在当前会话顺序实施。任务1–12、独立整条分支审查及必修修复已完成，最终37项自动回归通过，Windows Development/Shipping完整包已交付本地。

## Global Constraints

2026-10-18 工作区分工：本会话的美术工作在 `D:/eleme/.worktrees/city-courier-art`、分支 `feat/city-courier-art` 进行，基于任务8提交 `2395e0d`。另一个 Codex 在 `D:/eleme/.worktrees/city-courier` 实现玩法代码。本工作区聚焦任务10及其资产、制作脚本和美术文档；仅在本工作区修改和生成文件，不共享原工作区的地图、Blueprint、构建输出或缓存。涉及相同地图、Blueprint 或运行时接口的改动，在集成时核对双方修改，不能直接覆盖另一方版本。

- 基线采用 UE 5.6，Windows x64；首版单人离线游戏，键鼠输入，固定傍晚与晴朗天气。
- 地图可游玩区域约 600 × 600 米；4 家商户、8 个收餐点、至少两条可骑行近路；首版至少打磨一段约 150 米长的商业街。
- 同屏上限初设为 6 辆背景车和 12 名行人；不模拟完整城市交通系统。
- 同时仅有一个活动订单；手机显示 3 个候选订单；金额全部用整数分保存；初始余额为 ¥0。
- 取餐、交付与休息站操作要求步行且距离目标不超过 2 米；骑行速度不高于 3 km/h 才能下车。
- 初始前进最高速度 36 km/h，倒车最高速度 6 km/h；满电基础续航按实际移动距离计算为 3 公里。
- 动力升级为 42 / 48 km/h，价格 ¥60 / ¥120；操控累计提升 10% / 20%，价格 ¥40 / ¥80；续航升级为 4 / 5 公里，价格 ¥50 / ¥100。
- 订单时限为 clamp(ceil(90 + L / 5), 120, 300) 秒；L 为候选列表刷新时的道路总长度，单位米；接单后固定。
- 时限用尽后进入 60 秒宽限期；等于正常时限计为超时，等于最终失败时刻时失败；订单基础收入为 ¥16–¥32。
- 撞击速度低于 8 km/h 不扣损；有效损伤为 clamp(round((v - 8) × 1.5), 1, 30) 个百分点，间隔至少 1 秒；完整度归零失败。
- 1080p 中档平均不低于 30 FPS、1% low 不低于 24 FPS 是目标，60 FPS 为优化目标；以打包后 3 分钟固定路线实测验收。
- 不恢复进行中的订单、位置或电量；重启回休息站、满电，已结算收入和升级保留；存档版本为 1。
- 原创或来源明确的可用素材；不购买素材，不复制 GTA 资产；角色、车辆和代表性街段占位物必须在最终验收前替换。
- UE 引擎、编译缓存、打包输出不提交 Git；不修改引擎源文件，不擅自清理用户其他项目来腾出磁盘。
- 中间阶段允许占位资源，但只能标记为原型；缺少实际 UE 编译、运行或打包证据时，明确记录未验证。

## Review Focus

1. 手机、地图、暂停和骑行切换交错：油门不残留，手机/地图不暂停计时，Esc 暂停计时；任务 8 的输入切换功能用例覆盖。
2. 交付交互与超时同帧发生：无论事件到达顺序如何，最终失败边界不能结算；任务 2 的精确边界测试和任务 7 的世界时钟测试覆盖。
3. 保存过程中退出、文件损坏或磁盘不可写：保留有效旧档，明确提示未保存，不假报成功；任务 3 的故障注入和任务 8 的界面验证覆盖。
4. 车辆侧面堵住、玩家跌落地图或恢复点失效：拒绝不安全下车，恢复到有效安全点，不损坏订单数据；任务 6 的碰撞功能测试和任务 9 的恢复测试覆盖。
5. 编辑器正常、打包缺字体/地图/软引用资产：中文不缺字，所有目的地可达，关闭编辑器后仍可配送；任务 10 的资产验证和任务 12 的完整打包回归覆盖。

## 文件组织与职责

下列路径相对于仓库根目录 `D:/eleme`；`{h,cpp}` 表示一对同名头文件与源文件，实施时分别创建。每个文件只承担表中职责；测试位于 `Source/CityCourier/Tests/`，使用 `WITH_DEV_AUTOMATION_TESTS` 排除发行构建。调用编辑器API的夹具另加 `WITH_EDITOR`，UnrealEd等依赖仅在Editor目标添加，避免Development或Shipping游戏目标链接编辑器模块。

| 文件 / 目录 | 职责 |
| --- | --- |
| `CityCourier.uproject`、`Source/CityCourier.Target.cs`、`Source/CityCourierEditor.Target.cs` | 引擎关联、游戏和编辑器目标 |
| `Source/CityCourier/CityCourier.Build.cs`、`CityCourier.{h,cpp}` | 单一 Runtime 模块与必要依赖 |
| `Source/CityCourier/Core/DeliveryTypes.h`、`DeliveryRules.{h,cpp}` | 数据契约、无场景依赖订单状态机 |
| `Source/CityCourier/Profile/PlayerProfile.h`、`ProfileRules.{h,cpp}`、`ProfileSaveGame.{h,cpp}`、`ProfileRepository.{h,cpp}`、`ProfileSubsystem.{h,cpp}` | 金额、升级、存档事务与持久玩家资料 |
| `Source/CityCourier/World/RoadGraph.{h,cpp}`、`CityDefinition.{h,cpp}`、`DeliveryLocation.{h,cpp}` | 道路、地点与商户/收餐点数据 |
| `Source/CityCourier/Player/CourierCharacter.{h,cpp}`、`CourierController.{h,cpp}`、`CourierGameMode.{h,cpp}`、`CourierInteractable.h` | 角色、输入、控制切换、通用交互协议 |
| `Source/CityCourier/Vehicles/ScooterPawn.{h,cpp}`、`ScooterMovementComponent.{h,cpp}`、`VehicleRules.{h,cpp}`、`RiderRecoveryComponent.{h,cpp}` | 车辆物理近似、电量与安全恢复 |
| `Source/CityCourier/Delivery/DeliverySubsystem.{h,cpp}`、`DeliveryNavigationSubsystem.{h,cpp}` | 世界中的配送流程、导航与规则事件编排 |
| `Source/CityCourier/World/RiderStation.{h,cpp}` | 休息站充电、升级与救援入口 |
| `Source/CityCourier/UI/CourierHUDWidget.{h,cpp}`、`CourierPhoneWidget.{h,cpp}`、`CourierMapWidget.{h,cpp}`、`CourierMenuWidget.{h,cpp}`、`CourierSettings.{h,cpp}` | 中文 HUD、订单、地图、结算/升级菜单、设置 |
| `Source/CityCourier/Ambient/AmbientTrafficManager.{h,cpp}`、`AmbientVehicle.{h,cpp}`、`AmbientPedestrian.{h,cpp}`、`CourierAudioComponent.{h,cpp}` | 有限背景人车、游戏音效与总音量 |
| `Content/CityCourier/{Maps,Data,Input,Blueprints,UI,Art,Audio}/` | 可烘焙资源，全部游戏资产保持在项目命名空间下 |
| `Content/Python/`、`tools/art/`、`AssetsSource/` | 编辑器制作脚本、Blender 脚本、原创源资产 |
| `scripts/` | 环境检查、构建、资产制作、测试和打包入口 |
| `docs/testing/`、`docs/art/`、`docs/environment.md`、`README.md` | 验收证据、素材清单、环境和操作说明 |

## 依赖与统一验证入口

顺序：1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10 → 11 → 12。任务 2–4 有独立规则测试，但最终产品依赖同一组接口，默认顺序开发减少集成改动。

任务 1 实现以下 PowerShell 入口。命令均从仓库根目录运行，`-EngineRoot` 接受实际安装路径，缺省读取进程环境变量 `CITYCOURIER_UE_ROOT`；未设置时探测 Launcher 清单并验证版本，找不到则明确报错，不猜测一个有效路径。

```powershell
.\scripts\Check-Environment.ps1
.\scripts\Build.ps1 -Target Editor
.\scripts\Run-EditorScript.ps1 -Script Content/Python/bootstrap_project.py
.\scripts\Test.ps1 -Filter CityCourier.Smoke -Headless
.\scripts\Package.ps1 -Configuration Development
```

构建调用引擎 `Engine/Build/BatchFiles/Build.bat`；打包调用 `RunUAT.bat BuildCookRun`，Win64、build/cook/stage/pak/archive 并包含 prerequisites。入口对非零退出码终止，日志保存在 `Saved/Validation/`，打包目录为 `Artifacts/Windows/<Configuration>/`，不提交这些目录。

测试入口调用 `UnrealEditor-Cmd.exe`，传入 `-unattended -nop4`、测试过滤器和 `-ReportExportPath`；纯规则及只检查资源加载的测试可以用 `-NullRHI`，涉及实际画面、物理或输入的功能测试不能用它代替验证。退出条件等待 automation 队列完成；同时检查报告中匹配测试数大于 0、无失败、无未执行测试，不把空报告或仅进程退出码 0 当作成功。设可配置运行超时，超时明确失败并只停止本脚本启动的进程。需要场景的功能测试通过 `Test.ps1 -Filter ...` 运行，不使用 `-Headless`。

手工视觉、手感和连续游玩验证保存在 `docs/testing/acceptance.md`，逐项记录实际结果、构建提交号与截图路径；未执行项保持未勾选。每个任务只运行相关测试，最终统一回归。

## Task 1：能启动并打包的最小 UE 工程

**Files:** 创建工程和模块文件、`Config/DefaultEngine.ini`、`Config/DefaultGame.ini`、`.gitignore`、上述五个 `scripts/*.ps1`、`Content/Python/bootstrap_project.py`、`Content/CityCourier/Maps/L_CourierCity.umap`、`Source/CityCourier/Tests/SmokeTests.cpp`、`docs/environment.md`、`docs/testing/acceptance.md`、`docs/art/asset-manifest.csv`（任务10定义的表头）。

**Interfaces:** `Build.ps1 -Target Editor|Game`；`Test.ps1 -Filter <prefix> [-Headless]`；`Run-EditorScript.ps1 -Script <relative-path>`；`Package.ps1 -Configuration Development|Shipping`。内部统一解析绝对路径，避免空格路径参数损坏。

- [x] **1.1 核验环境并建立引擎。** 检查 UE 5.6 正式版本、MSVC、SDK、.NET、剩余空间及实际安装组件大小；本次只读检查发现 MSVC 14.44.35207、SDK 10.0.22621.0，未找到 UE。使用 Epic 官方安装渠道，仅选 Windows 所需组件；记录最终补丁、编译器、SDK 与安装目录。账户登录由用户操作，磁盘不足时只请求必要目录选择，不能自行删文件或把未安装状态写成完成。
- [x] **1.2 创建原生最小工程。** 配置 UE 5.6 和 Runtime 模块，启用 Enhanced Input、Python Editor Script Plugin（仅制作期），写 PowerShell 入口与忽略规则。`Build.ps1 -Target Editor` 必须得到真实构建成功；UBT 拒绝编译器时按其诊断安装匹配组件，不压制兼容性错误。
- [x] **1.3 创建并保存启动关卡。** `bootstrap_project.py` 在 UE 编辑器中创建地面、光照、PlayerStart 和 `L_CourierCity`，配置默认启动地图；再次运行不得删除已有游戏资产或复制重复 Actor。通过运行脚本检查 `.umap` 确实生成并能载入。
- [x] **1.4 加入必要的 smoke 测试并运行。** `CityCourier.Smoke.StartupMap` 断言 `/Game/CityCourier/Maps/L_CourierCity` 能加载，且包含有效 PlayerStart；故意指定不存在的测试过滤器时 runner 必须报失败。执行统一入口的 smoke 命令，输出至少 1 项成功测试且无失败。
- [x] **1.5 打包并启动最小游戏。** 执行 `Package.ps1 -Configuration Development`；实际启动打包 exe，确认地面、光照、输入响应可见。无图形验证条件时记录阻碍，不能把打包成功视为已观察到画面。
- [x] **1.6 提交。** `git add -- CityCourier.uproject Source Config scripts Content/Python Content/CityCourier/Maps .gitignore docs/environment.md docs/testing/acceptance.md docs/art/asset-manifest.csv`；`git commit -m "build: establish runnable UE5 Windows project"`。

## Task 2：可独立验证的订单状态与收益规则

**Files:** 创建 `Core/DeliveryTypes.h`、`Core/DeliveryRules.{h,cpp}`、`Tests/DeliveryRulesTests.cpp`、`Tests/TestSupport.h`（均在 `Source/CityCourier/` 下）。

**Interfaces:** `EDeliveryStatus {Idle,AwaitingPickup,Delivering,Completed,Failed}`；`EDeliveryFailure {None,Expired,Damaged,Cancelled,Rescue}`；`FDeliveryOffer {FGuid InstanceId; FName MerchantId,DestinationId; int64 BaseFeeCents; double DeadlineSeconds;}`；`FDeliveryState` 包含 Offer、Status、Failure、ElapsedSeconds、IntegrityPercent、LastDamageElapsedSeconds。`FDeliveryRules` 静态接口：`Accept(State&, const Offer&) -> bool`、`Pickup(State&, FName) -> bool`、`Advance(State&, double DeltaSeconds) -> void`、`ApplyImpact(State&, double SpeedKmh) -> void`、`Deliver(State&, FName, int64& OutPayoutCents) -> bool`、`Cancel(State&, EDeliveryFailure) -> bool`、`CalculateDeadline(double RouteMeters) -> double`；实际使用以上完整类型名。

- [x] **2.1 先写行为测试。** 测试辅助 `MakeOffer(double DeadlineSeconds=120, int64 BaseFeeCents=2000)` 创建全新 ID、固定 merchant/destination。核心断言写为：

```cpp
// CityCourier.DeliveryRules.Boundaries（各断言使用独立订单实例）
TestEqual(TEXT("短路线"), FDeliveryRules::CalculateDeadline(0), 120.0);
TestEqual(TEXT("600米"), FDeliveryRules::CalculateDeadline(600), 210.0);
TestEqual(TEXT("上限"), FDeliveryRules::CalculateDeadline(2000), 300.0);
FDeliveryState State;
const FDeliveryOffer Offer = MakeOffer();
TestTrue(TEXT("接单"), FDeliveryRules::Accept(State, Offer));
TestTrue(TEXT("取餐"), FDeliveryRules::Pickup(State, Offer.MerchantId));
State.IntegrityPercent = 80; // 测试夹具设置损餐后的状态
FDeliveryRules::Advance(State, 120.0);
int64 Payout = 0;
TestTrue(TEXT("宽限期可交付"), FDeliveryRules::Deliver(State, Offer.DestinationId, Payout));
TestEqual(TEXT("超时折扣"), Payout, int64(960));
TestFalse(TEXT("不可重复交付"), FDeliveryRules::Deliver(State, Offer.DestinationId, Payout));
// 其他独立用例：119.999秒 => 1600分。
// 179.999秒可交付；180秒为 Failed/Expired，拒绝交付且 OutPayoutCents=0。
// 未取餐、错误目标、重复交付拒绝；终态不再接受伤害或第二次取消。
// 7.99km/h 无损；8km/h 扣1；28km/h 扣30；1秒内第二次撞击不扣。
```

- [x] **2.2 验证失败。** 执行 `Build.ps1 -Target Editor` 和 `Test.ps1 -Filter CityCourier.DeliveryRules -Headless`；缺失实现导致的编译失败可作为首次红灯，但需记录真实原因，随后测试必须能执行。
- [x] **2.3 实现规则。** 每个动作先验证状态与目标；所有时间判断使用同一已用时间；负数或非有限 DeltaSeconds 不改变状态，非法路线距离的 CalculateDeadline 返回 -1，调用者据此拒绝候选；过期先于交付。金额用 int64 中间乘积，`BaseFeeCents * IntegrityPercent * (Late ? 60 : 100) / 10000`，只做一次整数截断；拒绝超出1600–3200分、时限不在120–300秒或无效ID的 Offer。LastDamageElapsedSeconds 初始化为负无穷，确保第一次撞击立即生效。
- [x] **2.4 运行并补充边界证据。** 相同过滤器全部成功，加入恰好 1 秒伤害冷却、完整度归零、连续多次终结、负数/NaN 时间输入和非法基础金额；验证有效金额 1601 分、99% 完整度、超时得到 950 分。
- [x] **2.5 提交。** `git add -- Source/CityCourier/Core Source/CityCourier/Tests`；`git commit -m "feat: implement delivery rules and settlement boundaries"`。

## Task 3：玩家经济、升级交易与可靠存档

**Files:** 创建 `Profile/` 表中各文件与 `Tests/ProfileTests.cpp`、`Tests/ProfileRepositoryTests.cpp`。

**Interfaces:** `EUpgradeKind {Power,Handling,Battery}`；`FPlayerProfile {int32 Version=1; int64 BalanceCents,TotalEarnedCents; int32 CompletedCount,FailedCount,PowerLevel,HandlingLevel,BatteryLevel; float MasterVolume; int32 QualityLevel;}`；`FSettlement {FGuid InstanceId; bool bCompleted; int64 PayoutCents;}`；`EProfileLoadResult {NewProfile,Loaded,RecoveredBackup,NeedsReset}`。`UProfileSubsystem : UGameInstanceSubsystem` 提供 `LoadProfile() -> EProfileLoadResult`、`ResetProfileAfterConfirmation() -> bool`、`ApplySettlement(const FSettlement&) -> bool`、`TryPurchase(EUpgradeKind) -> bool`、`SaveProfile() -> bool`、`GetProfile() const -> const FPlayerProfile&`、`UpdateSettings(float Volume,int32 Quality) -> bool`，广播 OnProfileChanged / OnSaveFailed。`FProfileRules::GetUpgradeCost(EUpgradeKind,int32 CurrentLevel) -> int64` 返回下一等级价格，非法或满级返回 -1；`Validate(const FPlayerProfile&) -> bool` 校验快照。

`FProfileRepository::Load(FPlayerProfile&, FString& OutError) -> EProfileLoadResult`、`Save(const FPlayerProfile&, FString& OutError) -> bool`。在 `ProfileRepository.h` 定义可注入 `IProfileStorage`：`Read(const FString& Name,TArray<uint8>& Bytes) -> bool`、`Exists(const FString& Name) const -> bool`、`WriteAtomic(const FString& Name,const TArray<uint8>& Bytes) -> bool`、`Preserve(const FString& Name) -> bool`。生产实现仅操作固定存档目录，WriteAtomic负责临时文件与替换；测试实现模拟读、写、替换失败。Preserve将不兼容原文件保留为带时间和唯一后缀的文件。

- [x] **3.1 写失败测试。** `CityCourier.Profile` 验证初始0分、一次完成加2000分、同实例重复结算无效、失败只加失败数；余额不足6000分不能买动力1级，6000分恰好买到且余额0；第二级12000分，满级不可再扣款；操控4000/8000、电池5000/10000分分别验证。
- [x] **3.2 写存档故障测试并运行红灯。** `CityCourier.ProfileStorage` 对比完整往返、主档损坏回退备份、两个文件都损坏、Version=2、非法余额/等级、写临时文件失败、替换主档失败；`Test.ps1 -Filter CityCourier.Profile -Headless` 必须观察到失败。
- [x] **3.3 实现资料与交易。** 新资料的金额、计数、等级均为0，音量0.8、画质1；结算实例防重在会话内保存；重启不恢复订单，避免恢复已结算实例。升级与设置通过统一接口验证后修改并保存，失败保存仍保留有效内存资料；重试保存不得重放交易。设置限制音量0–1、质量0–2；非法数据保持原值。
- [x] **3.4 实现版本1快照与故障处理。** 存档在用户 Saved/SaveGames 下；使用 SaveGame 序列化内存数据，文件层写临时文件、验证可读、保留有效备份后替换主档。只保留已验证有效的备份；未知版本或损坏原文件先重命名保留，用户选择新档时才建立默认档。迁移不是本任务需求。
- [x] **3.5 运行绿灯。** 重跑 Profile 过滤器，断言每种失败都返回错误且主档或备份仍可恢复；测试临时目录必须在项目 Saved/Tests 内，不触碰玩家正式进度。
- [x] **3.6 提交。** `git add -- Source/CityCourier/Profile Source/CityCourier/Tests`；`git commit -m "feat: persist rider profile and validate upgrade transactions"`。

## Task 4：连通的中国街区灰盒与地点数据

**Files:** 创建 `World/RoadGraph.{h,cpp}`、`World/CityDefinition.{h,cpp}`、`World/DeliveryLocation.{h,cpp}`、`Tests/RoadGraphTests.cpp`、`Tests/CityDataTests.cpp`、`Content/Python/build_city.py`、`Content/CityCourier/Data/DA_CityDefinition.uasset`；修改 `L_CourierCity.umap`。

**Interfaces:** `FRoadNode {int32 Id; FVector Position;}`、`FRoadEdge {int32 From,To; double LengthMeters; bool bBidirectional;}`、`FRouteResult {bool bReachable; double LengthMeters; TArray<FVector> Points;}`；`FRoadGraph::FindRoute(int32 From,int32 To) const -> FRouteResult`、`FindNearestNode(FVector) const -> int32`、`Validate(TArray<FString>& Errors) const -> bool`。`UCityDefinition` 存储道路、4商户、8收餐点、各地点入口节点、32个组合的费用和休息站数据；`ADeliveryLocation` 暴露 LocationId、LocationKind、InteractionPoint。地点坐标为 UE 厘米，进入计时公式前换算米。

- [x] **4.1 写失败测试。** `CityCourier.Roads` 验证单向、双向、断路、同点、不同权重最短路、重复节点、无效边、负长度；10000厘米应为100米。`CityCourier.CityData` 验证4个商户、8个收餐点、唯一ID、费用1600–3200分，32个商户/目的地组合全部可达。
- [x] **4.2 运行红灯。** 构建并运行 `Test.ps1 -Filter CityCourier.Roads -Headless`，随后 CityData 过滤器，保存失败原因。
- [x] **4.3 实现道路图和资源校验。** 使用 Dijkstra 按非负道路长度寻路；位置到道路入口的连接长度计入总长；任一无效关键数据给出地点ID和原因，不生成无效候选订单。
- [x] **4.4 制作灰盒关卡。** 编辑器脚本在约600 × 600米的区域内建立环路、支路、两条可骑行近路、四区域、休息站和地点标记；采用真实尺度碰撞体，数据和场景Actor共用稳定ID；多次制作只更新带项目制作标签的生成内容，保护人工资产。
- [x] **4.5 验证。** 运行 Roads / CityData 测试，编辑器中检查地图尺度、12个地点落地、道路不穿建筑；可达性数学测试不能代替之后任务6的实际骑行检查。
- [x] **4.6 提交。** `git add -- Source/CityCourier/World Source/CityCourier/Tests Content/Python/build_city.py Content/CityCourier/Data Content/CityCourier/Maps`；`git commit -m "feat: build connected courier district and delivery locations"`。

## Task 5：第三人称步行、相机与交互协议

**Files:** 创建 `Player/` 表中各文件、`Tests/InteractionTests.cpp`、`Content/CityCourier/Input/IMC_Courier.uasset`、`IA_Move.uasset`、`IA_Look.uasset`、`IA_Sprint.uasset`、`IA_Interact.uasset`、`Blueprints/BP_CourierCharacter.uasset`、`Maps/L_MovementTest.umap`；修改模块依赖与关卡默认GameMode。

**Interfaces:** `ICourierInteractable::CanInteract(ACourierCharacter*, FText& OutReason) const -> bool`、`Interact(ACourierCharacter*) -> bool`、`GetInteractionPriority() const -> int32`。`ACourierCharacter::TryInteract() -> bool`；`ACourierController::GetCourier() const -> ACourierCharacter*`、`SetInterfaceMode(ECourierInterfaceMode) -> void`，枚举含 Gameplay / Phone / Map / Pause / Station / Settlement。

- [x] **5.1 建立功能用例。** `CityCourier.Interaction` 创建近/远交互对象，199cm可交互、201cm拒绝、被墙遮挡拒绝；当前订单对应地点优先于休息站、车辆和其他地点，同优先级取最近对象；无目标不显示过期提示。
- [x] **5.2 运行红灯。** `Test.ps1 -Filter CityCourier.Interaction`；需真实测试世界，不能用空测试替代角色位置和碰撞。
- [x] **5.3 实现步行与相机。** CharacterMovement 管理走/跑，SpringArm 管理第三人称镜头碰撞，Enhanced Input绑定WASD/鼠标/Shift/E；设置界面切换的输入清空接口供任务8使用。Manny可作为本阶段有明确标记的临时资产，填入已有素材清单并标记占位。
- [x] **5.4 验证。** 自动用例通过，在 `L_MovementTest` 检查窄巷相机、门口交互提示、碰撞和奔跑；记录人工观察。
- [x] **5.5 提交。** `git add -- Source/CityCourier/Player Source/CityCourier/Tests Source/CityCourier/CityCourier.Build.cs Content/CityCourier/Input Content/CityCourier/Blueprints Content/CityCourier/Maps Config docs/art/asset-manifest.csv`；`git commit -m "feat: add courier movement camera and validated interactions"`。

## Task 6：电动车驾驶、上下车和电量基础

**Files:** 创建 `Vehicles/ScooterPawn.{h,cpp}`、`ScooterMovementComponent.{h,cpp}`、`VehicleRules.{h,cpp}`、`Tests/VehicleRulesTests.cpp`、`Tests/ScooterFunctionalTests.cpp`、`Blueprints/BP_Scooter.uasset`、`Input/IA_Brake.uasset`；修改 CourierCharacter / CourierController / IMC_Courier / L_MovementTest。

**Interfaces:** `AScooterPawn::TryMount(ACourierCharacter*) -> bool`、`TryDismount() -> bool`、`SetDriveInput(float Throttle,float Steering,bool bBrake) -> void`、`GetSpeedKmh() const -> float`、`GetBatteryFraction() const -> float`、`ApplyUpgrades(const FPlayerProfile&) -> void`、`SetBatteryFraction(float) -> void`；事件 `OnEffectiveImpact(float NormalClosingSpeedKmh)`。`FVehicleRules::ConsumeDistance(float Fraction,double DistanceMeters,int32 BatteryLevel) -> float`、`GetMaxSpeedKmh(int32 PowerLevel) -> float`、`GetRangeMeters(int32 BatteryLevel) -> double`。

- [x] **6.1 写规则与场景测试。** `CityCourier.VehicleRules`：3000/4000/5000米耗尽对应满电，倒车距离同样扣电，0电不驱动，动力上限36/42/48；非法等级查询回退等级0并记录错误，持久化层仍拒绝该数据。`CityCourier.Scooter`：速度3可下车、3.01拒绝，左右堵住拒绝，单侧安全使用该侧。
- [x] **6.2 运行红灯。** 分别运行 VehicleRules（Headless）与 Scooter（有测试世界），记录预期失败。
- [x] **6.3 实现街机车辆与控制切换。** 使用 PawnMovementComponent 的有扫掠移动和滑动响应，按秒计算加速度及速度相关转向；前进制动到低速后S倒车，上限6km/h；Space强制动。上车附着骑手并关闭冲突碰撞，下车先探测落点再解除附着；失败不改变控制权。
- [x] **6.4 实现电量与镜头。** 实际物理位移计里程，瞬移标记排除；低于20%提示事件在跨阈值时发出；车体视觉侧倾不改变稳定碰撞体，相机防穿墙并自动回正。有效撞击速度来自法向接近速度。
- [x] **6.5 验证。** 自动用例成功；手动以30与60FPS上限分别骑同一路段，比较加速与里程无明显帧率依赖；骑遍两条近路与所有地点，测试正撞、擦墙、倒车、下车和高速镜头。
- [x] **6.6 提交。** `git add -- Source/CityCourier/Vehicles Source/CityCourier/Player Source/CityCourier/Tests Content/CityCourier/Blueprints Content/CityCourier/Input Content/CityCourier/Maps`；`git commit -m "feat: add scooter driving possession and battery rules"`。

## Task 7：把接单、取餐、配送与结算接入世界

**Files:** 创建 `Delivery/DeliverySubsystem.{h,cpp}`、`Tests/DeliveryWorldTests.cpp`；修改 DeliveryLocation、CourierGameMode、ScooterPawn 和 ProfileSubsystem 集成。

**Interfaces:** `UDeliverySubsystem : UTickableWorldSubsystem` 提供 `RefreshOffers(FVector RiderLocation) -> bool`、`GetOffers() const -> const TArray<FDeliveryOffer>&`、`TryAccept(FGuid) -> bool`、`TryPickup(FName,ACourierCharacter*) -> bool`、`TryDeliver(FName,ACourierCharacter*) -> bool`、`TryCancel(EDeliveryFailure) -> bool`、`ApplyRecoveryPenalty() -> void`、`GetState() const -> const FDeliveryState&`；广播 `OnDeliveryChanged` 与 `OnDeliveryFinished(const FSettlement&)`。世界层持有统一、暂停感知的时钟，每次事件先同步ElapsedSeconds再调用规则；不将Tick与交互事件分别累加计时。

- [x] **7.1 写端到端失败用例。** `CityCourier.DeliveryWorld` 验证3个有效候选、一次仅接1单、真实步行距离/对应商户检查、取餐后目标切换、交付一次加钱并刷新候选；撞击只有骑乘且已取餐才损餐。使用可控时钟验证截止时刻先交互后Tick和先Tick后交互结果一致。
- [x] **7.2 运行红灯。** `Test.ps1 -Filter CityCourier.DeliveryWorld`；报告必须实际执行世界用例。
- [x] **7.3 实现生成和状态编排。** 候选从32个有效组合中随机抽取3个不同组合，实例ID全新；时限按玩家到商户再到目的地路线计算。统一转发结果给 ProfileSubsystem，成功/失败终结仅一次，保存失败不重发结算；允许人工取消和救援原因。
- [x] **7.4 验证配送闭环。** 自动用例成功；使用暂时的开发交互面板完成至少1单准时、1单宽限期、1单失败，检查Profile变化；该临时面板在任务8中移除。
- [x] **7.5 提交。** `git add -- Source/CityCourier/Delivery Source/CityCourier/World Source/CityCourier/Player Source/CityCourier/Profile Source/CityCourier/Vehicles Source/CityCourier/Tests`；`git commit -m "feat: connect delivery loop to city interactions and profile"`。

## Task 8：中文 HUD、手机、导航、暂停与存档提示

**Files:** 创建 UI 表中全部 C++ 文件、`Delivery/DeliveryNavigationSubsystem.{h,cpp}`、`Tests/NavigationTests.cpp`、`Tests/InputModeTests.cpp`、`Content/CityCourier/UI/WBP_CourierHUD.uasset`、`WBP_CourierPhone.uasset`、`WBP_CourierMap.uasset`、`WBP_CourierMenu.uasset`、`Fonts/F_CourierChinese.uasset`；修改输入资产、Controller、ProfileSubsystem集成。

**Interfaces:** `UDeliveryNavigationSubsystem::GetActiveRoute() const -> const FRouteResult&`、`RefreshRoute(FVector RiderLocation) -> bool`；0.5秒检查一次偏航，距离路径超过15米或目标变化时重新计算。`UCourierHUDWidget::RefreshFromState() -> void` 等界面仅读取系统状态；菜单把交易和订单操作交回既有接口。`UCourierSettings::Apply(const FPlayerProfile&) -> void` 应用音量和UE画质档。

- [x] **8.1 写导航与输入失败测试。** `CityCourier.Navigation` 验证取餐前导航到商户、取餐后到顾客、不可达无穿墙路线、偏航重算。`CityCourier.InputModes` 验证驾驶→手机→地图→暂停→游戏交错：油门归零，手机/地图保持时间推进，Pause暂停，恢复后需新输入才加速。
- [x] **8.2 运行红灯。** 使用 Navigation 和 InputModes 过滤器，验证真实计时与控制输入。
- [x] **8.3 制作中文界面。** HUD包含目标、计时/宽限期、完整度、余额、速度、电量；手机3个候选和活动单操作；地图绘制道路/导航线/地点；结算逐项展示基础收入、损餐、超时和实际收入。嵌入有明确许可且覆盖所用汉字的字体，记录来源，字号按1080p可读制作。
- [x] **8.4 完成暂停、设置和存档恢复交互。** Esc暂停及继续；音量0–1、画质低中高通过Profile保存；退出明确提示进行中的订单将丢弃。根据 EProfileLoadResult 提示恢复结果，NeedsReset 时经用户选择调用 ResetProfileAfterConfirmation；保存失败显示“本次进度未保存”并提供重试。初次运行 NewProfile 直接默认资料，不显示错误。
- [x] **8.5 验证。** 自动用例成功；在1280×720和1920×1080查看无文字裁剪，手动检查保存错误/重试；测试商户后导航切换与离路重算，移除临时开发面板。
- [x] **8.6 提交。** `git add -- Source/CityCourier/UI Source/CityCourier/Delivery Source/CityCourier/Player Source/CityCourier/Profile Source/CityCourier/Tests Content/CityCourier/UI Content/CityCourier/Input docs/art/asset-manifest.csv`；`git commit -m "feat: add Chinese courier UI navigation and pause flow"`。

## Task 9：休息站升级、充电与安全恢复

**Files:** 创建 `World/RiderStation.{h,cpp}`、`Vehicles/RiderRecoveryComponent.{h,cpp}`、`Tests/RiderStationTests.cpp`、`Tests/RecoveryTests.cpp`、`Input/IA_Recover.uasset`；修改菜单、输入映射、关卡、ScooterPawn和Controller。

**Interfaces:** `ARiderStation::BeginCharge(ACourierCharacter*) -> bool`、`CancelCharge() -> void`、`TryPurchase(EUpgradeKind,ACourierCharacter*) -> bool`；`URiderRecoveryComponent::RecordSafeLocation(const FTransform&) -> void`、`TryRecover() -> bool`、`TryRescue() -> bool`。安全点采样检测地面、车体空间和道路邻近，恢复失败候选回退到休息站安全出生点。

- [x] **9.1 写失败测试。** `CityCourier.Station`：充电4.99秒仍未满、5秒满；取消不充电；离开2米范围取消；升级电池保持百分比。`CityCourier.Recovery`：长按不足2秒无动作；恢复增加5秒且可能立即失败，不修复完整度、不增减电量；过期安全点回退；没电且离站超过20米才可救援，救援有活动单失败一次、无活动单不增失败数。
- [x] **9.2 运行红灯。** 分别运行 Station / Recovery 世界测试。
- [x] **9.3 实现交互与恢复。** 升级菜单调用Profile交易并立即将等级应用到车辆；充电期间要求步行持续有效且车辆静止，展示5秒进度。恢复和救援用碰撞检测定位，保持骑乘状态或安全步行状态，清空旧速度和输入；救援取消订单、回站、充满。
- [x] **9.4 验证。** 自动用例成功；手动把车卡在窄巷、角色放到地图下方、耗尽电量后检查可恢复。购买三类升级观察速度、转向、续航变化，退出重启仍有效。
- [x] **9.5 提交。** `git add -- Source/CityCourier/World Source/CityCourier/Vehicles Source/CityCourier/Player Source/CityCourier/UI Source/CityCourier/Tests Content/CityCourier/Input Content/CityCourier/Maps`；`git commit -m "feat: add rider station upgrades charging and recovery"`。

## Task 10：原创骑手、电动车与中国街区美术

2026-10-18 用户确认的美术约束：素材预算 ¥0，只用免费且许可适用的外部资产或自行制作的资产；沿用中国南方城市街区、晴朗傍晚、写实风格。免费外部资产可作为人物、服装、建筑和材质的制作基础，逐项保存来源和许可；不以必须全手工原创限制素材选择，也不以简单几何体组合替代写实验收。先制作一名骑手、一辆电动车及 50–100 米样板街段，检查实际步行、骑行画面与性能，再扩展至原定约 150 米商业街。该样板不替代任务10的最终验收。

**Files:** 创建 `tools/art/build_courier_assets.py`、`AssetsSource/CityCourier/` 内的 `.blend` 与导出文件、`Content/Python/import_art.py`、`Content/CityCourier/Art/{Characters,Scooter,Architecture,StreetProps,Materials}/`、`Blueprints/ABP_Courier.uasset`、`Tests/ArtAssetTests.cpp`；更新 `docs/art/asset-manifest.csv`、地图和角色/车辆Blueprint。

**Interfaces:** `import_art.py` 读取资产清单并导入稳定目标路径，不更名运行时引用；骑手动画状态使用 `GroundSpeed`、`bRiding`、`SteeringAmount`，车辆座位和手脚定位使用明确命名的 RiderSeat / LeftGrip / RightGrip / LeftPedal / RightPedal 锚点。清单列为 asset_id,source_path,ue_path,author,source_url,license,is_placeholder。

- [x] **10.1 建立美术验收和资产验证。** `CityCourier.ArtAssets` 检查关键角色、车体、动画、材质、字体和地图软引用均可加载，关键资产清单 is_placeholder=false；先对当前占位资产运行并确认失败。截图验收角度固定为骑手近景、骑行背面、商业街远景、夜市和小区入口。
- [x] **10.2 制作原创角色和车辆。** 准备Blender环境并用保留源文件的制作流程建模：有明确人体比例的骑手、头盔/工作服/餐箱、电动车车架/车轮/灯具；制作站立、行走、跑步、骑行骨架动画，导入UE并配置Animation Blueprint。资产必须实际查看，不以脚本执行成功代替美术质量判断。
- [x] **10.3 制作并布置模块化街区。** 原创商铺门面、居民楼、围墙、招牌、摊位、空调、道路边缘与杂物；以约150米商业街为重点打磨，其他区域保持同样材质和比例。中文招牌使用原创名称，固定傍晚光照；资产与源码一并记录，使用外部素材时先核对许可并填清单。
- [x] **10.4 导入与验证。** 编辑器脚本可重复执行且不破坏手工场景；自动资产测试通过，生成固定角度截图并逐一检查材质、缩放、穿模和中文。步行与骑行跑过所有配送入口，确保装饰没有堵路；主要资产仍明显占位时本任务不通过。
- [x] **10.5 提交。** `git add -- tools/art AssetsSource Content/Python/import_art.py Content/CityCourier/Art Content/CityCourier/Blueprints Content/CityCourier/Maps docs/art Source/CityCourier/Tests`；`git commit -m "art: create courier scooter and Chinese district assets"`。

## Task 11：有限人车、环境音与反馈音效

**Files:** 创建 `Ambient/` 表中全部文件、`Tests/AmbientTests.cpp`、`Content/CityCourier/Audio/`、`Content/CityCourier/Art/Ambient/`、`tools/art/build_ambient_assets.py`、`Content/CityCourier/Blueprints/BP_AmbientVehicle.uasset`、`BP_AmbientPedestrian.uasset`；修改地图、资产清单和角色/车辆事件绑定。

**Interfaces:** `AAmbientTrafficManager::UpdatePopulation(FVector RiderLocation) -> void`；背景车沿独立车道点巡行，行人沿人行道点巡行，不占用订单道路节点身份。`UCourierAudioComponent::PlayDeliveryCue(ECourierAudioCue) -> void`，枚举 Accepted/PickedUp/Delivered/Failed；`SetVehicleSpeed(float) -> void`控制车辆循环音频，音量由统一设置应用。

- [x] **11.1 写失败用例。** `CityCourier.Ambient` 验证同屏不超过6车/12人，远距实体停用不持续堆积；前方玩家或车辆阻挡时停车，移开可恢复，连续5分钟无成倍生成；不存在订单/资料修改副作用。
- [x] **11.2 运行红灯并实现背景逻辑。** 管理器复用有限实体；探测制动距离按车速变化；行人遇障停下或换下一段，不穿建筑，不引入受伤/死亡系统。用制作脚本生成原创低复杂度背景轿车，行人复用任务10骨架及步行动画并制作普通服装变体；源资产放入 AssetsSource/CityCourier 并更新清单。
- [x] **11.3 制作并接入声音。** 添加有明确来源的城市底噪、电动车行驶、制动与四种配送提示音；原创录制或合成音源保留制作说明，外部音源进入清单。事件只触发一次，菜单暂停时停止对应世界声音，恢复后平顺继续。
- [x] **11.4 验证。** Ambient测试通过；实机站在车道、窄巷和商户入口检查避障，走完配送路线监听音量与重复提醒；背景人车不阻断全部配送入口。
- [x] **11.5 提交。** `git add -- Source/CityCourier/Ambient Source/CityCourier/Tests Content/CityCourier/Audio Content/CityCourier/Art/Ambient Content/CityCourier/Blueprints Content/CityCourier/Maps tools/art/build_ambient_assets.py AssetsSource docs/art`；`git commit -m "feat: add bounded street traffic pedestrians and audio"`。

## Task 12：画质、性能、完整回归与 Windows 交付

**Files:** 创建 `Config/DefaultScalability.ini`、`scripts/Measure-Performance.ps1`、`docs/testing/performance.md`、`docs/testing/release-checklist.md`、`README.md`；修改设置、打包配置、必要性能问题所属文件，完成 `docs/testing/acceptance.md`。

**Interfaces:** `Measure-Performance.ps1 -CsvPath <actual-frame-capture.csv> -> performance summary`，只处理真实打包版本采样；统计平均FPS=`1000/平均帧时ms`，1% low=`1000/最慢1%帧时的均值ms`，记录CPU/GPU/内存/显存采样来源。不把编辑器或无渲染测试用作性能证据。

- [x] **12.1 配置三档画质与烘焙范围。** 低档轻量光照/阴影、中档软件Lumen、高档提高质量；硬件光追默认关闭。配置字体、地图、DataAsset及软引用进入cook；编辑器脚本和源制作文件不进入运行时依赖。
- [x] **12.2 运行完整自动回归。** `Build.ps1 -Target Editor`，`Test.ps1 -Filter CityCourier`；报告包含各任务测试且全部执行成功。定位并修复真实失败，修复后运行相关测试；最终没有待修缺陷时不反复扩大测试范围。
- [x] **12.3 打包并验证。** 分别 `Package.ps1 -Configuration Development` 与 Shipping；关闭编辑器，从打包目录启动。连续至少5单覆盖4家商户，访问全部8点，验证收入/升级重启保留、当前订单丢弃、中文字体、音效、画质、救援、退出确认和保存错误提示；连续游玩20分钟无崩溃或流程卡死。
- [x] **12.4 测量并优化。** 固定1080p中档、同一3分钟路线，在目标RTX3060 Ti机器实测；首次shader加载与稳定段区分记录。平均FPS≥30、1% low≥24；不达标先分析CPU/GPU瓶颈，再调整实例化、阴影、纹理、实体更新和Lumen质量，保持已确认玩法。优化后重测相同路线，保留真实数值。
- [x] **12.5 完成发行资料。** README包含启动exe位置、完整键鼠操作、存档位置/备份/重置、设置、当前限制；素材清单与对应许可文件完整；release-checklist每项附证据。交付工程源码、资产、Windows完整目录与报告，不能只交付exe或未编译源码。
- [x] **12.6 审查与提交。** 按已选执行流程做最终审查；所有必修问题修复并验证后，`git add -- Config scripts/Measure-Performance.ps1 docs/testing README.md`，另行精确暂存本任务实际改动的源码和资产；`git commit -m "release: validate and package first courier gameplay slice"`。不执行远程发布或上传。

## 覆盖自审与交接

| 设计范围 | 负责的任务 |
| --- | --- |
| UE环境、工程、打包 | 1、12 |
| 订单、超时、损餐、收益 | 2、7 |
| 余额、升级、存档和错误恢复 | 3、8、9 |
| 600米城区、地点、道路、导航 | 4、8 |
| 步行、驾驶、相机、输入 | 5、6、8 |
| 电量、充电、卡住恢复、救援 | 6、9 |
| 角色/车辆/城市美术、中文字体 | 8、10 |
| 人车与音效 | 11 |
| 画质、性能、全流程验收、发行资料 | 12 |

计划自审检查：范围映射完整；金额为分、距离为米/UE厘米的边界明确；任务接口采用同一命名；关键事件使用暂停感知的同一时钟；五项Review Focus均有对应验证。外部前提是能安装和运行UE、足够磁盘以及能够制作和检查真实资产；已完成步骤依据实际提交和验证记录勾选，任务12的最终结果见发行检查表。

参考：Epic 的 [自动化测试运行](https://dev.epicgames.com/documentation/unreal-engine/run-automation-tests-in-unreal-engine)、[测试报告](https://dev.epicgames.com/documentation/unreal-engine/review-test-results-in-unreal-engine)、[BuildCookRun 打包](https://dev.epicgames.com/documentation/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine)、[Python编辑器脚本](https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-editor-using-python)；实施时以所安装UE5.6的命令和API实际验证为准。

**执行方式已确认：** 当前会话顺序实施，最终阶段集中独立审查。用户于2026-10-17回复“1”。

