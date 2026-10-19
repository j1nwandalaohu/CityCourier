# 全城室内美术接入

三个样板已扩展到全部 293 栋建筑，覆盖住宅、办公楼、四处商家和单层驿站。正式地图 `L_CourierCity` 与预览地图 `L_CourierArtPreview` 均使用新装修材质；家具、门框、踢脚线、墙裙、扶手、灯具、开关、插座和空白房号底板在原生详情加载时创建。

`AssetsSource/CityCourier/Interiors/inventory.json`、`layout.py` 和 `DA_Buildings` 的建筑编号、楼层、房间、门洞及导航锚点未改变。源布局逐栋检查通过：18,321 个房间、15,697 扇门、245,269 个附加美术实例。驿站使用紧凑家具比例，保留相同的 120 cm 中央站立区、45 cm 路线预留和完整门扇扫掠检查；普通楼栋保留已验收样板比例。

## 运行方式

新数据资产 `/Game/CityCourier/Art/Interiors/DA_InteriorFinish` 只保存附加美术，按建筑编号索引，引用实际模型以供打包。它不替代功能建筑目录。模型和碰撞类型相同的陈设使用 HISM，门扇外观使用三组 ISM，不创建逐件美术 Actor。详情沿用原有 30 m 加载、45 m 退出和占用/配送目标保留策略；本次没有把 24 万件陈设同时常驻到地图中。

详情准备时替换旧家具组合，避免两套家具叠放。家具及扶手保持阻挡；薄装修无碰撞。灯具使用真实矩形光源：1800 lm，商铺 4500 K、住宅/驿站 3800 K、办公楼 5000 K，带阴影，650 cm 衰减，25 m 绘制距离及 5 m 淡出。天花灯和半层平台墙灯均随真实灯具朝向。房号文字继续由原功能生成。

近处活动门和缓存门使用同一套三种门扇，沿原铰链和原角度运动；活动时删除相应外观实例，恢复代理时重建其保存姿态。碰撞仍使用原来的独立门片/代理。卸载详情恢复常驻代理的显示和原碰撞；再次进入只生成一套陈设。缺失资产使详情准备和配送准入失败，不删除建筑结构。

## 地图交接与验证

接入以 main 当时尚未提交的店面地图为输入，原始 SHA-256 为 `eda4d903f687196c7756cc940a9bbaa0c09eacaddde7dc5a609dd281a30654fb`，副本保留在美术工作树 `Saved/main-storefront-map-before-rollout.umap`。原生迁移脚本先生成完整可进入楼体，再隐藏旧实心体及浅店景，保留道路、玩法标记和无关对象；门口已有街边装饰按原功能的净空规则调整，记录在 `entrance-prop-placement.json`。main 的其他未提交脚本、文档和快捷方式不包含在此次美术提交中。

实际验证使用原生角色运动、楼梯踏步、家具/扶手阻挡和真实开门，采用加速固定步长。两张地图分别走访 293 栋、18,321 个房间，打开 15,697 扇门，全部返回街道，每张累计约 181.96 km。运行记录随最终验证保存在 [city-interior-validation](city-interior-validation/)；源几何记录为 `InteriorArt/city-plan-validation.json`。

此结果验证可达性和资产接入，不代表墙钟性能验收或已达到照片级。[正式地图四张实拍](city-interior-validation/index.html) 已导出并检查，运行 `EditorScript-20261019-102348-5d8b7b0b` 退出 0，原始图片 1600×1000，拍摄期间源码和地图哈希未改变。原有样板图片见 [样板相册](interior-samples/index.html)，其原图及记录保留。

本轮 Editor 编译通过，Python 33/33、UE 自动化 93/93 通过，已有免费资源 116 个文件的 SHA-256 校验通过。最终全量行走运行是 `EditorScript-20261019-102156-30ab47ad`，测试门具有真实所属建筑，使用实际新门扇。完整证据及资产/源码哈希见 [verification.json](city-interior-validation/verification.json)。

## 重建

```powershell
./scripts/Build.ps1 -EngineRoot D:/sdk/UE_5.6
& D:/sdk/Python/Python314/python.exe -m tools.interiors.build_city_finish
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_city_finish.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/build_enterable_city.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/validate_city_finish.py -EngineRoot D:/sdk/UE_5.6
./scripts/Test.ps1 -EngineRoot D:/sdk/UE_5.6 -Headless
```

地图重建仍由一方串行执行，不覆盖其他会话正在编辑的 `.umap`。独立检查正式地图实例化外观可运行 `capture_city_finish.py -Gui`；它只临时准备三个站点的原生详情，以实际地图灯光拍摄，不写地图、不添加补光或逐件预览模型，截图属于编辑器画面验证。
