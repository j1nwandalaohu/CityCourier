# 功能完善版 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** 完成操作反馈、接单距离与路线预览、驿站／找车导航、持久化订单历史，并交付可运行的 Windows 游戏包。

**Architecture:** 扩展现有规则与子系统接口，界面消费同一份校验结果、路线快照和结算快照。候选预览与实际导航分离；历史与余额在同一次资料更新中保存。继续复用原生 UMG 和既有 Blueprint 包装，不调整美术资源。

**Tech Stack:** UE 5.6.1、C++、UMG、UE Automation、Windows PowerShell、现有原子文件存储。

**Spec:** `docs/superpowers/specs/2026-10-18-city-courier-function-improvements-design.md`（用户已要求继续实现，书面设计获确认）。

## Global Constraints

- 功能工作树 `D:/eleme/.worktrees/city-courier`；基线 `a9c8073`；不修改另一个美术 worktree，不改组件名称、挂点、资源路径或城市布局。
- 历史保留最近 20 单；驿站车辆三维距离不超过 12 米；操作失败提示显示 3 秒。
- 驿站要求玩家距入口不超过 2 米、视线可达、已下车；车辆无人骑乘、速度小于 0.1 km/h。
- 失败提示候选限制在玩家 5 米内；可操作目标仍使用原有优先级及可达性判断。
- 保持单订单、计费、期限、恢复和救援规则；活动订单不跨重启保存。
- 手机、地图、预览与历史均不暂停世界；Esc 暂停冻结配送计时。
- 距离小于 1 km 显示取整米数，否则显示一位小数公里；候选以刷新位置估算，接单不改变已显示费用和时限。
- 导航每 0.5 秒更新；车辆目标移动超过 2 米、偏航超过 15 米或目标变化时重算；沿有效路线到终点不超过 2 米提示到达。
- 存档版本 1 升为 2；名称快照各最多 64 个 UTF-16 单元、不拆代理对；payload 不超过 65536 字节。
- 引擎路径 `D:/sdk/UE_5.6`；构建、自动化测试、打包和资源操作串行；测试与打包验证使用隔离 UserDir。
- 每项先写行为测试，观察预期失败，再实现并验证；不把编译错误当作预期行为失败。
- 最后进行一次独立的整体代码审查；不自动合并、推送或修改用户真实存档。

## Review Focus

1. 手机卡片刷新后点击旧按钮：按订单实例 ID 拒绝失效候选，不能误接新的同索引订单（Task 2）。
2. 找车过程中车辆被救援移动或玩家已上车：更新实际目标，正确退出找车，不丢失配送任务（Task 3）。
3. 版本 1 备份、迁移后首次保存失败、未知更高版本：保留原始进度，不自动重置或覆盖（Task 4）。
4. 世界未暂停时订单恰在预览／历史页超时：结算正常、只记录一次，界面返回不恢复失效状态（Tasks 4–5）。
5. 最长中文／代理对名称、20 条历史及低分辨率滚动：存档不超限，文本和按钮可访问，金额不因舍入矛盾（Tasks 4–5）。

## Shared verification commands

以下命令在功能工作树运行。每个 Task 的 RED 和 GREEN 都先构建再测试；每条命令须成功才运行下一条。

```powershell
./scripts/Build.ps1 -Target Editor -EngineRoot D:/sdk/UE_5.6
./scripts/Test.ps1 -Filter CityCourier.FunctionalUpgrade -EngineRoot D:/sdk/UE_5.6
```

新测试统一采用 `CityCourier.FunctionalUpgrade.<TaskGroup>.<Case>` 名称。开发时将 Filter 缩至具体 TaskGroup；完成任务时运行该任务组及受影响既有组。新 API 可先加入返回失败的最小声明／桩以运行行为测试；不得提前实现规则。RED 必须对应断言失败；崩溃、环境错误、零测试和报告缺失都不是有效 RED。

### Task 1: 操作结果与驿站停车校验

**Files:** Create `Source/CityCourier/Core/CourierActionResult.h`; modify `Player/CourierCharacter.{h,cpp}`, `Player/CourierController.{h,cpp}`, `Vehicles/ScooterPawn.{h,cpp}`, `World/RiderStation.{h,cpp}`, `UI/CourierHUDWidget.cpp`, `UI/CourierMenuWidget.cpp`; create `Tests/ActionFeedbackTests.cpp` (all under `Source/CityCourier/`).

**Interfaces:** Produce `FCourierActionResult { bool bSucceeded; FText Reason; }`; add `ACourierCharacter::TryInteractWithFeedback()`, `AScooterPawn::TryMountWithFeedback(ACourierCharacter*)`, `TryDismountWithFeedback()`, `ARiderStation::CheckService(ACourierCharacter*) const`, `BeginChargeWithFeedback(ACourierCharacter*)`, `TryPurchaseWithFeedback(EUpgradeKind, ACourierCharacter*)`, all returning `FCourierActionResult`. Keep existing bool entrypoints as wrappers. Controller provides `void ShowActionFeedback(const FText&)` and `FText GetActionFeedback() const` with a 3-second world-time expiry. Station exposes `FText GetServiceMessage() const` for asynchronous charging cancellation.

- [x] Write `FunctionalUpgrade.Feedback` tests: denied actions return distinct nonempty reasons without mutating state; valid target still wins over rejected target; no candidate beyond 5 m; player distance 2 m allowed and above rejected; vehicle station distance 12 m allowed and above rejected; moving vehicle and occupied vehicle rejected; charging interrupted on invalid distance; insufficient funds and max level produce their own reasons.
- [x] Build and run `CityCourier.FunctionalUpgrade.Feedback`; observe those behavioral failures against the old behavior/minimal API shells.
- [x] Implement the result APIs with shared validation and stable failure ordering from spec §3. Preserve collision, dismount placement and movement rules. Connect controller feedback and menu messages; no tick-driven toast spam; clear menu messages on mode change.
- [x] Rebuild and run `CityCourier.FunctionalUpgrade.Feedback`, `CityCourier.Interaction`, `CityCourier.Station`, `CityCourier.Scooter`, and `CityCourier.InputModes`. Check concrete test count and every result; never accept an empty report.
- [x] Commit explicit changed files as `fix: explain rejected courier actions and require nearby station parking`.

### Task 2: 候选距离与不干扰导航的预览数据

**Files:** Modify `Core/DeliveryTypes.h`, `Delivery/DeliverySubsystem.{h,cpp}`, `Player/CourierController.{h,cpp}`; create `Tests/OfferPreviewTests.cpp`.

**Interfaces:** Add `FDeliveryOffer::EstimateOrigin` (`FVector`), `PickupRoute`, `DeliveryRoute` (`FRouteResult`). Distances come from their `LengthMeters`; total is their sum. Add `bool UDeliverySubsystem::GetOfferPreview(FGuid, FDeliveryOffer& OutOffer) const` returning a copy, and `AScooterPawn* GetScooter() const`. Controller adds `bool PreviewOffer(FGuid)`, `void CloseOfferPreview()`, `const FDeliveryOffer* GetOfferPreview() const`, and `bool AcceptOffer(FGuid)`; old `CourierOrder(int32)` remains a wrapper resolving the current ID. Preview storage belongs to controller, not navigation.

- [x] Write `FunctionalUpgrade.Offers` tests: stored pickup plus delivery route length equals the route used for deadline generation; routes include entrance connectors; preview copy does not accept order/change navigation; refresh invalidates old IDs; accepting stale ID does not select a new index; active order rejects refresh; acceptance preserves displayed fee/deadline.
- [x] Build and run `CityCourier.FunctionalUpgrade.Offers`; observe missing behavior failures.
- [x] Populate both routes once during candidate generation from the same origin; retain current 3-candidate selection and 120–300 second deadline rule. Resolve preview/acceptance by ID and return precise feedback on invalid or stale requests. Reject unreachable candidates and leave gameplay route unchanged during preview.
- [x] Rebuild and run `CityCourier.FunctionalUpgrade.Offers`, `CityCourier.DeliveryWorld` and `CityCourier.DeliveryRules`; inspect reported success counts.
- [x] Commit as `feat: add stable offer distance estimates and route preview data`.

### Task 3: 独立驿站与找车导航

**Files:** Modify `World/CityDefinition.{h,cpp}`, `Delivery/DeliveryNavigationSubsystem.{h,cpp}`, `Player/CourierController.cpp`; create `Tests/NavigationModeTests.cpp`.

**Interfaces:** Consume Task 2 `UDeliverySubsystem::GetScooter()`. Add `UCityDefinition::FindRouteToPosition(FVector From, FVector To) const -> FRouteResult`. Add `ECourierNavigationMode { Order, Station, ParkedBike }`; navigation provides `FCourierActionResult SetNavigationMode(ECourierNavigationMode)`, `ECourierNavigationMode GetNavigationMode() const`, `FText GetTargetLabel() const`, `double GetRemainingMeters() const`, `FVector GetTargetPosition() const`, `bool HasArrived() const`. Existing `GetActiveRoute`, `RefreshRoute`, `UpdateNavigation`, `DistanceFromRoute` remain supported.

- [x] Write `FunctionalUpgrade.Navigation` tests: station override survives pickup and order finish; new acceptance selects Order; empty Order mode has no target; ParkedBike rejects riding, exits on mount and restores Order if active; moved/recovered bike reroutes; route progress decreases remaining distance; unreachable connector reports failure; preview leaves navigation mode/route intact.
- [x] Test arbitrary-endpoint routing including same edge, opposite ends, bidirectional and one-way edges, disconnected graph and nonfinite coordinates. Assert returned route respects direction and length, including endpoint connectors; do not silently route a one-way edge backwards.
- [x] Build and run `CityCourier.FunctionalUpgrade.Navigation`; observe the intended failures.
- [x] Implement endpoint projection and graph traversal; validate connector visibility/collision consistently with existing route checks. Update remaining progress every 0.5 seconds, reroute at spec thresholds; handle invalid vehicle and changed world state without stale pointers. Recompute from actual player pawn location after mode switch; maintain order state independently.
- [x] Rebuild and run `CityCourier.FunctionalUpgrade.Navigation`, `CityCourier.Navigation` and `CityCourier.Roads`.
- [x] Commit as `feat: navigate independently to station and parked scooter`.

### Task 4: 历史记录、一次性结算与存档迁移

**Files:** Create `Profile/OrderHistory.h`, `Profile/OrderHistory.cpp`, `Tests/OrderHistoryTests.cpp`, `Tests/ProfileMigrationTests.cpp`; modify `Profile/PlayerProfile.h`, `Profile/ProfileRules.{h,cpp}`, `Profile/ProfileRepository.cpp`, `Profile/ProfileSubsystem.cpp`, `Delivery/DeliverySubsystem.cpp`; add `Tests/Fixtures/ProfileV1.sav` and its provenance text (paths under `Source/CityCourier/`).

**Interfaces:** Define reflected `FOrderHistoryEntry` with SaveGame fields: `FGuid InstanceId`, `FString MerchantName, DestinationName`, `FName MerchantId, DestinationId`, `FDateTime SettledAtUtc`, `bool bCompleted, bLate`, `uint8 FailureCode` (explicit mapping to existing failure enum), `int64 BaseFeeCents, PayoutCents`, `double ElapsedSeconds, DeadlineSeconds`, `int32 IntegrityPercent`. Add `TArray<FOrderHistoryEntry> FPlayerProfile::OrderHistory`; default Version 2. Extend `FSettlement` with `FOrderHistoryEntry History` while retaining existing identity/completed/payout members and validating their agreement. Add `FProfileRules::UpgradeToCurrent(FPlayerProfile&, bool& bMigrated) -> bool` and `ValidateSupported(const FPlayerProfile&) -> bool`; retain `Validate` for current version. Add `FOrderHistoryRules::MakeEntry(const FDeliveryState&, int64, const FString&, const FString&, FDateTime) -> FOrderHistoryEntry`, `Validate(const FOrderHistoryEntry&) -> bool`, `DescribeIncome(const FOrderHistoryEntry&) -> FText`.

- [x] Before changing profile types, capture a real version-1 binary fixture using the baseline serializer in isolated storage; record exact values and producer commit. Include nonzero balance, counts, upgrades and nondefault settings. This fixture must not be generated by the upgraded serializer.
- [x] Write `FunctionalUpgrade.History` tests: every terminal cause and late completion records once; repeated callbacks and save retries do not duplicate money/history; 21st entry removes oldest without changing totals; UTC timestamp and insertion order survive roundtrip; failed entries earn 0; integer payout explanation sums exactly; longest names and surrogate boundaries serialize within 65536-byte payload.
- [x] Write `FunctionalUpgrade.Migration` tests: actual v1 fixture preserves every old field; v1 backup recovery migrates; first migration write failure preserves readable original; retry succeeds without duplicate history; unknown future version is preserved and not auto-overwritten; duplicate IDs, invalid enum, NaN and contradictory completion/cause are rejected.
- [x] Build and run both new groups; observe missing history/migration behavior before implementing it. Update existing test helpers to construct valid complete settlement snapshots, without weakening their old assertions.
- [x] Implement old-version validation before upgrade, current-version validation after upgrade, atomic snapshot updates and bounded history. Validate source before truncating names; preserve surrogate pairs. Load marks migrated data dirty and attempts normal save; repository backup recognition accepts supported v1 without calling it corrupt. Keep current-session deduplication set unbounded by the 20-row history cap; no history replay API.
- [x] Build and run `CityCourier.FunctionalUpgrade.History`, `CityCourier.FunctionalUpgrade.Migration`, `CityCourier.Profile`, `CityCourier.ProfileStorage`, `CityCourier.DeliveryWorld`.
- [x] Commit as `feat: persist bounded delivery history with safe profile migration`.

### Task 5: 手机、地图、历史与导航反馈集成

**Files:** Modify `UI/CourierPhoneWidget.{h,cpp}`, `UI/CourierMapWidget.{h,cpp}`, `UI/CourierHUDWidget.{h,cpp}`, `UI/CourierMenuWidget.cpp`, `Player/CourierController.{h,cpp}`; create `UI/CourierHistoryWidget.{h,cpp}`, `Tests/FunctionalUpgradeUITests.cpp`.

**Interfaces:** Consume Tasks 1–4 APIs. `UCourierHistoryWidget::RefreshFromState()` reads Profile.OrderHistory; `void OpenEntry(FGuid)` renders an immutable selected entry. History is a phone subpage, preview uses existing Map mode with controller-owned preview state; no extra input-mode enum needed. Add phone actions for refresh, preview by stored card ID, history, station, bike, restore Order and back. Map adds return button and reads preview only for full map, never minimap.

- [x] Write `FunctionalUpgrade.UI` tests: card button binds the displayed ID; preview return restores phone; world time advances while history/preview open; expiration closes overlays into Settlement once; Esc from either pauses and does not revive stale preview; repeated key transitions neither accept another order nor leave movement enabled in menus.
- [x] Build and run `CityCourier.FunctionalUpgrade.UI`; observe missing behavior failures using actual native widget/controller paths.
- [x] Implement two-leg preview with separate colors and legend, independent parked-bike marker, updated target/remaining-distance HUD and navigation buttons with disabled explanations. Label offer estimates by refresh origin and retain visible deadline. Add history empty state, scrollable 20-row list, per-entry detail and back. Use shared `DescribeIncome` in history and settlement; menu cancellation messages use Task 1 result.
- [x] Rebuild and run `CityCourier.FunctionalUpgrade.UI` and `CityCourier.InputModes`. Inspect real rendered 1280×720 and 1920×1080 UI for readable Chinese, scrolling and reachable buttons, including 20 rows/longest names; verify actual font glyph coverage. No asset redesign.
- [x] Commit as `feat: expose route previews navigation shortcuts and order history`.

### Task 6: 回归、实际游戏验证与发布交付

**Files:** Modify `Validation/PackagedValidationDriver.{h,cpp}` only for development-only opt-in validation if needed; update `README.md`, `docs/playing.txt`, `docs/testing/acceptance.md`, `docs/testing/release-checklist.md`; create `docs/testing/function-improvements-validation.md` and focused evidence under `docs/testing/evidence/function-improvements/`.

**Interfaces:** All production paths from Tasks 1–5; validation driver must call the same controller/widget action methods used by players. Fault injection and fixture setup are test-only and explicitly reported.

- [ ] Extend opt-in validation to assert actual preview/history transitions, station distance denial, mount feedback, navigation switching, each terminal record and reload in isolated UserDir. Run new checks before final package release; if any behavior fails, reproduce with a focused failing test before fixing.
- [ ] Run `./scripts/Build.ps1 -Target Editor -EngineRoot D:/sdk/UE_5.6`, then `./scripts/Test.ps1 -Filter CityCourier -EngineRoot D:/sdk/UE_5.6`; inspect all report entries and reject zero/missing/failed cases. Expected: complete suite passes.
- [ ] Run `./scripts/Package.ps1 -Configuration Development -EngineRoot D:/sdk/UE_5.6`, then the same command with `Shipping`. Expected: both package successfully with their complete runtime folders.
- [ ] Launch isolated packaged runs, one at a time. Complete a real pickup/delivery with preview first, detour to station, find parked bike, verify history income and reload preservation; separately exercise failure, old-save migration and write failure with fixtures. Check physical Tab/M/Esc and mouse buttons in Shipping. Record which tests use fixtures and which are actual travel; do not claim an unperformed performance or listening test.
- [ ] Update player instructions and evidence with build/report paths, exact pass counts and remaining limitations; commit as `test: verify and document functional improvements release`.
- [ ] Run one fresh whole-change reviewer through the execution skill, compare against spec and this Review Focus, fix material findings with failing regression tests and a passing complete suite. Repackage and repeat affected packaged checks when production code changes. Preserve the local feature branch and runnable artifacts; no unsolicited merge/push.

## Plan self-review and handoff

All four feature groups and spec §9 acceptance criteria map to Tasks 1–6. Dependencies are 1 → 2/3/5 (feedback), 2 → 3/5 (vehicle and preview), 4 → 5 (history), all → 6 (delivery). Shared API names are defined above; preview ownership remains separate from navigation and profile history. Legacy test prefixes were checked against source; empty tests cannot pass.

Recommended execution: Native, implement all tasks in this session, with one independent whole-change review at the end. These tasks share controller and subsystem interfaces; keeping one implementer reduces coordination overhead. Plan review is pending; no product code has been changed by writing this plan.
