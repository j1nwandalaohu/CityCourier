# Enterable Buildings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 全城所有建筑、所有实际楼层和每个房间可以通过真实门与楼梯步行进入，并完成室内配送。

**Architecture:** 单一确定性布局定义编译为空间、几何与步行图；基础壳体与门代理常驻，近处激活交互及室内陈设。保留道路图，以建筑入口连接室内路线，沿用现有交互、订单、存档与角色动作。

**Tech Stack:** UE 5.6、C++、UE Editor Python、Python 3.14 unittest、既有 Blender 美术工具与项目材质、PowerShell 构建验证。

**Spec:** `docs/superpowers/specs/2026-10-18-city-courier-enterable-buildings-design.md`。

执行方式：本会话直接实施，使用 executing-plans、TDD 和单次最终独立审阅。用户明确授权完成设计后直接实现，不再请求设计、计划或阶段确认。新工作树 `D:/eleme/.worktrees/enterable-buildings`，分支 `feat/enterable-buildings`；保留其他工作树和未提交修改。

## Global Constraints

- 全量清单包括沿街模块、商家、配送入口附属建筑与驿站；不能以生成成功项反推期望集合。
- 无缝步行；门 E、2 米，90 度、0.6 秒；净宽普通 100／入口 120 cm，净高 220 cm，通路 120 cm，通行净高 240 cm；踏高不超过 17 cm，踏深至少 28 cm。
- 基础几何常驻；30 米预加载、45 米卸载距离；单栋并发准备、八栋未占用细节缓存；占用与进入请求不可驱逐。
- 所有门无永久锁，双侧操作，扫掠防夹；图路径可以要求玩家打开门，物理碰撞不能直接忽略关闭门。
- 既有 LocationId、费用、唯一结算、宽限期 60 秒、v2 历史与未来版本保护继续有效；时限按规范分项，范围 120–300 秒。
- 新资产写入独立 Buildings 命名空间，不覆盖另一个会话正在维护的原美术源文件；地图集成只在本工作树进行。
- Editor／Game 构建、UE 测试、Editor 脚本、打包及实机运行在本会话中串行执行；不得终止其他会话的 UE 进程。

## Review Focus

1. 非均匀缩放、镜像前向与不同层高：实例级净空及门洞一致，不能仅验证未缩放模板（Task 1、3、7）。
2. 打开的门经过细节卸载或 Actor 转换：角度、碰撞和本次会话状态保持，不露空洞或移除楼板（Task 2、4、7）。
3. 同一 XY 的上下楼层与隔门目标：不能误交付或误报到达，跨楼路线先从当前室内退出（Task 5、6）。
4. 配置后才出现的地点、失效资源与重复注册：订单绑定正确，失败不得开始计时或重复结算（Task 4、6）。
5. 清单漏楼、重复 ID、模板生成失败或地图重建：全量覆盖失败而不是缩减期望总数，旧安全碰撞只在单栋迁移成功后替换（Task 1、3、7）。

## File structure and interfaces

- `tools/interiors/layout.py`：纯 Python 确定性布局，`compile_building(seed: dict) -> dict`、`validate_building(building: dict) -> list[str]`，包含稳定 rooms／doors／nodes／edges、结构 boxes 和装饰 placements。
- `tests/test_interior_layout.py`：尺寸、跨层、可达性、编号、缩放和非法输入测试。
- `Source/CityCourier/Buildings/CourierBuildingDefinition.{h,cpp}`：`UCityBuildingCatalog` 及建筑／楼层／房间／门／通行节点数据和校验。
- `Source/CityCourier/Buildings/CourierDoor.{h,cpp}`：`ACourierDoor` 实现 `ICourierInteractable`；`Configure`、`TryToggle`、`AdvanceDoor` 和可保存的角度／目标状态。
- `Source/CityCourier/Buildings/CourierBuildingSubsystem.{h,cpp}`：加载目录、空间与门查询、当前位置、缓存、动态地点；`Locate(FVector)`、`FindIndoorRoute`、`GetNearbyDoors`。
- `Source/CityCourier/Buildings/CourierBuildingActor.{h,cpp}`：复用材质与实例化结构、楼梯、门代理、房间陈设，以及车辆入口阻挡。
- `Source/CityCourier/Buildings/CourierInteriorRouting.{h,cpp}`：纯室内寻路与路线分项，不依赖已加载的门 Actor。
- `Content/Python/build_enterable_city.py`：扫描两张地图与地点、冻结全量清单、编译数据、应用新外壳及空间、保存幂等地图。
- `Content/Python/validate_enterable_city.py`：地图清单、资产、动态与物理覆盖报告。
- `tools/art/build_enterable_assets.py`：沿用既有调色板与细节，生成可进入外壳与独立门扇，保留原资产。
- `Source/CityCourier/Tests/Building*.cpp` 与 `Source/CityCourier/Validation/BuildingValidation.*`：规则、真实场景和 Development 验证。
- 修改 `World/CityDefinition.*`、`World/CityPositionRouting.cpp`、`World/RoadGraph.h`、`Delivery/*`、`Player/CourierCharacter.cpp`、`UI/CourierMapWidget.cpp`：扩展入口／房间绑定、完整路线、有限候选交互、楼层地图与配送校验；保留旧 bool API 包装及室外无建筑数据的兼容行为。

## Task 1: Canonical layouts and full inventory

Produces: JSON-compatible building data with explicit centimeter geometry, stable IDs, room bounds, door links, floor heights, route nodes and structural boxes. Main entry, all rooms, stairs and roof access belong to the same graph.

- [x] Write `tests/test_interior_layout.py` first: missing compiler fails; valid A/B/C buildings have all rooms reachable and returnable; stair rise ≤17 and tread ≥28; every door references two real regions; wrong sizes／NaN／duplicate IDs／missing floors fail. Exercise all production width／depth／rotation cases from existing generator metadata.
- [x] Run `D:/sdk/Python/Python314/python.exe -m unittest discover -s tests -p test_interior_layout.py -v`; record expected RED for missing behavior.
- [x] Implement `compile_building` and `validate_building` in `tools/interiors/layout.py`; preserve source transforms, derive floors from actual visual variant, choose fitting layouts, enumerate independent expected IDs before geometry generation.
- [x] Run the same tests GREEN; retain a deterministic source layout sample under `AssetsSource/CityCourier/Interiors/` and commit.

## Task 2: Runtime data, doors and native collision behavior

Consumes Task 1's field schema. Produces native serializable catalog, pure indoor route search and a doorway actor with stable data identity.

- [x] Add UE tests for absent catalog and closed/opening/open/closing behavior, two-sided E, blocked motion stopping at last safe angle, pause, duplicate edges and correct cross-floor route. Start with a runtime asset-availability failure that compiles against existing UDataAsset APIs, then add typed tests alongside the new types.
- [x] Build Editor and run `./scripts/Test.ps1 -Filter CityCourier.Buildings -EngineRoot D:/sdk/UE_5.6`; preserve observed RED.
- [x] Implement definition validation, graph route lengths, `ACourierDoor` state machine with swept blocker checks and pose-preserving proxy transition; use existing unified player admission and feedback. Closed doors block strikes; moving doors are the only ticking doors.
- [x] Run focused UE tests GREEN and existing action tests; commit.

## Task 3: Formal assets and deterministic map migration

Consumes building schema and catalog reflection. Produces separately named exterior／door assets, compiled cooked catalog and both maps with complete structures.

- [x] Add inventory validation first: expect every canonical building, all floors, rooms and visible entrance annexes; deliberately omit／duplicate one item and verify failures. Run against current city to capture missing-interior RED.
- [x] Implement `tools/art/build_enterable_assets.py` and import path: remove solid cores from new asset variants, retain external detail and materials, create door pivots, genuine openings, stairwell／roof and centimeter dimensions. No modification of original source meshes.
- [x] Implement `Content/Python/build_enterable_city.py` using Task 1 and native types. Keep roads and location IDs; create canonical inventory with source provenance and idempotent managed tags; replace each original solid shell only after new instance validation succeeds.
- [x] Build/import with serialized editor calls. Run generator twice and verify stable IDs/counts, preserved unrelated actors, no duplicate meshes／doors／rooms; commit source and generated assets.

## Task 4: Spatial registry, structural rendering and detail lifecycle

Consumes compiled catalog, runtime definitions and migrated maps. Produces persistent real structures, safe entry, session door state, spatial candidates and correct dynamic point registration.

- [x] Write tests for 30／45 meter hysteresis, occupied-building protection, eight inactive cache entries, deferred ready／failed states, door proxy ↔ actor transition, unload/reload angle preservation, readmission, and vehicle-only entrance blocking. Watch the relevant cases fail.
- [x] Implement `ACourierBuildingActor` using instanced structural meshes and existing materials; `UCourierBuildingSubsystem` owns registry, activation, detail cache and door states. Retain shell collision, all floors and stairs when details unload. Nearby-index candidates replace full world door scans.
- [x] Bind room-owned delivery actors when registered and unbind when removed. Keep authoritative order state outside streamed Actors; regenerate data-derived markers and room signs.
- [x] Run focused tests and actual doorway／stair fixture checks GREEN; commit.

## Task 5: Full routes, floor navigation and previews

Consumes building spatial addresses and indoor paths. Produces composed room-to-room routes and full journey estimates without affecting running orders.

- [ ] Write regressions for same XY different floors, room-to-street-to-other-building path, closed but openable doors, immutable preview, FindBike from upstairs and unreachable geometry. Watch RED.
- [ ] Extend `FRouteResult` with road／indoor flat／stair meters, mandatory door count and floor/address metadata; preserve legacy length and point fields. `UCityDefinition` composes interior entry/exit segments with the existing graph; door traversability is distinct from collision-at-this-instant.
- [ ] Adapt navigation's remaining distance and floor-aware arrival, keep independent Order／Station／ParkedBike modes, and render the current floor's schematic and stairs on map/minimap with existing corrected world projection.
- [ ] Run routing, preview, map and functional suites GREEN; commit.

## Task 6: Physical indoor delivery and compatibility

Consumes complete route snapshots and room bindings. Produces room-aware pickup/delivery, honest timing and unchanged persistent progress format.

- [ ] Write regressions for wrong floor／room／closed door, newly registered pickup actor, duplicate registration, missing target resource, exact expiry and unique settlement, old v1/v2 profiles and address history. Observe missing address gating／timing RED.
- [ ] Extend existing locations with street anchor and room interaction address; keep existing LocationId and names' 64-unit validity. Implement complete deadline formula from spec with 120–300 bounds. Offers use a single snapshot; acceptance defensively checks reachability without excluding legitimately closed doors.
- [ ] Add spatial membership to physical interaction checks; preserve station 12 m parking and vehicle safety, character controls, pause, existing income and damage rules. Reset building membership on recovery and use existing safe external destination.
- [ ] Run focused and complete UE suites GREEN; commit.

## Task 7: Whole-city physical coverage, real play and packages

Consumes all previous outputs. Produces tracked evidence, runnable game artifacts and an honest final requirements status.

- [ ] First run full validation against an intentionally missing room／blocked stair／wrong transform and verify explicit failures without reducing expected totals.
- [ ] Implement `Content/Python/validate_enterable_city.py` and opt-in Development native driver. Enumerate every building and room; verify graph return paths and capsule geometry for each actual instance, then physically traverse entrances, stairs and doors to every room. Record source hashes, exact identities, counts, failed paths and state.
- [ ] Run complete Editor build／UE tests, Python layout tests, tooling and performance-calculation tests. Preserve final reports and RED→GREEN reproductions for new integration defects.
- [ ] Run actual packaged play for every layout class, highest-floor delivery, return to street／bike, repeated load cycle, door anti-clipping and character actions; validate history after restart. Produce Development and Shipping builds in this worktree. Verify the production startup map contains the same complete catalog and assets.
- [ ] Measure packaged 1080p medium on the fixed 3-minute route; report actual mean／1% low／memory and run 20-minute stability. Check ≥30 mean FPS and ≥24 FPS 1% low; optimize within scope if unmet.
- [ ] Run one independent whole-branch review per executing-plans; address Important findings with focused RED→GREEN tests, then green suite. Update `docs/testing/enterable-buildings-validation.md`, plan and ledger; commit and keep user launch artifacts／other worktrees intact. No remote push.

## Plan self-review

The seven tasks cover the spec's inventory, all layers/rooms, dimensions, doors, lifecycle, route and address interfaces, delayed delivery registration, formal art contract, safe per-building collision replacement, compatibility, performance and whole-city evidence. Dependencies use a single canonical schema; routing consumes catalog data rather than live door Actors. Implementation begins immediately under the user's explicit authorization; no further plan approval step.
