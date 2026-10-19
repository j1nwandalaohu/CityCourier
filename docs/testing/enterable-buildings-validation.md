# 全城可进入建筑验证（2026-10-19）

功能工作树 `D:/eleme/.worktrees/enterable-buildings`，分支 `feat/enterable-buildings`。建筑、道路、地点编号及既有存档格式保留；本报告区分全量物理检查、实际游玩和真实打包性能。

## 覆盖与结构

两张实际地图均包含 **293 栋建筑、1461 个实际楼层、292 处屋顶、18321 个房间／公共空间、15697 扇门、156316 个结构实例**。1753 是包含屋顶的楼层记录数。

原生 CharacterMovement 在每栋实际建筑内连续行走，经每一扇扫掠开合的门到达全部预期房间，再返回街道；每张地图累计 181958.927 米。该检查固定步长加速调度，不是墙钟游玩或性能测量。缺房间、错实际变换和堵楼梯会明确失败，预期总量不会随实际遗漏减少。

全部 881 处外门另行审计街道道具净空。主地图保留全部 70 个有碰撞的街道道具，将挡门的 10 个盆栽、长椅和摊位移到旁边；网格、碰撞、道具数量、建筑与道路不变。原／新坐标及 DoorId 见 [调整记录](../../AssetsSource/CityCourier/Interiors/entrance-prop-placement.json)。地图重建与验证均接入此项，重复执行不再移动已清空的道具。

逐楼／逐房预期与实到 ID、开门数量、返回状态、源哈希见 [coverage.json](evidence/enterable-buildings/coverage.json)。最终检查 `EditorScript-20261019-030932-d85f8d3e`，最终地图重建 `EditorScript-20261019-030914-02fb56bc`。

## 自动化和审查

- 原生 **92/92**，`Automation-20261019-031046-879bc3ba`，见 [automation.json](evidence/enterable-buildings/automation.json)。
- Python 布局／入口净空 **16/16**；工具链 **13/13**；平均帧时间、最慢 1%、无效数据拒绝等性能计算检查通过。
- 单次独立审查的两个 Important 问题均已 RED→GREEN：按脚下位置连续识别楼梯楼层，就近连接真实梯级；楼号／楼层／完整房号共享于标牌、地图、导航、订单和历史，全部 18321 个地址唯一。
- 实际通行发现并修复了外门向内堵路、上层实体台阶侵占下层净空、抬高室外目标高度、家具遮挡后门路径，以及保留的街道盆栽遮挡开门视线。门向外开，踏板厚 20 cm；家具和街道道具都没有用关闭碰撞来通过检查。
- 打包材质缺少实例化支持的 RED 已修复：使用 15 个独立 `Buildings/Materials/` 副本保存渲染用途标记，不修改原美术材质。

## 实际包与性能

Development `Package-Development-20261019-031112-3055dff5` 和 Shipping `Package-Shipping-20261019-031159-1800d4be` 完整构建成功，ExitCode=0。

固定性能路线从正式 PlayerStart 正常步行到附近六楼房间，下楼远离入口至少 52 米，验证细节卸载，再回一楼验证准备就绪及打开的房门状态恢复，然后继续往返至三分钟。不传送、不禁用碰撞、不加速世界。

`Interiors-Acceptance-20261019-031345-08926902/Performance`：30 秒预热后采集 180.005 秒、19037 帧，原生 CSV 来自真实已打包游戏。

| 项目 | 实测 |
| --- | --- |
| CPU / GPU / RAM | i5-13400F / RTX 3060 Ti 8 GB / 16 GB |
| 输出、画质 | 1920×1080、中档、83% 内部分辨率重建 |
| 渲染 | D3D12、软件 Lumen GI／反射开启，硬件光追关闭 |
| 平均 / 1% low | **105.76 / 72.47 FPS**，通过 30 / 24 目标 |
| GT / RT / GPU 平均 | 4.91 / 9.44 / 7.62 ms |
| 大于 33.33 / 50 ms 的帧 | 0 / 0 |
| 峰值进程工作集 | 2559.45 MB |
| RHI 纹理分配峰值 | 930.62 MB；不代表包含全部缓冲的总显存 |
| 准备耗时峰值 | 20.415 ms |
| 同时就绪建筑 / 活动门峰值 | 8 / 38 |
| 实际路径断言 | 六楼到达、卸载后重进、开门姿态保留，均通过 |

同一预检中的实际车辆上／下车、正确 CourierVehicle 通道和建筑外下车通过。人物动作检查在约 10 秒内通过真实起跳、空中手机下落、落地、轻拳准入和暂停／界面隔离。现有 3 段出拳资产保留；新增跳跃／落地／受击／跑步衔接等 7 段美术仍按原动作契约交付，本次不冒充动作视觉已全部完成。

Shipping 独立 UserDir 启动 30 秒，观察到完整场景工作集与存档初始化，Development 验证参数未生效。该环境没有可操作的原生顶层窗口，测试只清理本轮自有进程；不把它算作物理按键或正常菜单退出验收。实际画面与交互证据来自上述 Development 包。

## 长期实机与重启

最终 `Interiors-Acceptance-20261019-032038-f95e1bba` 通过：连续实际游玩 **1288.882 秒（21 分 28.882 秒）**，覆盖全部 10 类布局，完成六楼房间配送与两次返回车辆。结算 1 单、失败 0 单、收入 22 元，没有残留活动订单。

独立重启检查用时 12.126 秒，完成数量、收入和完整楼层房号历史均保留，出生位置在建筑外，活动订单为空。见 [巡检与峰值汇总](evidence/enterable-buildings/play/soak-summary.json)、[重启结果](evidence/enterable-buildings/reload/result.json)、[路线预览实拍](evidence/enterable-buildings/screenshots/route-preview.png)、[六楼送达实拍](evidence/enterable-buildings/screenshots/sixth-floor-delivery.png) 和 [重启历史实拍](evidence/enterable-buildings/screenshots/history-after-restart.png)。早期诊断轮次与失败巡检不计作最终通过。

## 复现与交付

先执行 `scripts/Build.ps1 -Target Editor -EngineRoot D:/sdk/UE_5.6`，需要重建地图时依次运行套件导入、目录构建、`build_enterable_city.py` 和 `validate_enterable_city.py`。已有完整资产可直接 `scripts/Package.ps1 -Configuration Development -EngineRoot D:/sdk/UE_5.6`。

完整实机入口 `scripts/Validate-Interiors.ps1`，或分开执行 `-Checks Preflight`、`-Checks Play`。前者验证车辆、固定性能和人物动作；后者连续游玩至少 1200 秒、覆盖 10 类布局、完成最高层配送及两次返回车辆，再重启验证历史。长距离巡检使用正常按住 Shift 的游戏 API；固定性能路线保持普通步行。脚本检查原生 JSON、真实帧数据及覆盖数量，不能只靠进程退出码判定。所有轮次使用独立 `-UserDir`，不读写玩家真实进度。

交付文件及完整包哈希见 [package-manifest.json](evidence/enterable-buildings/package-manifest.json)。142 份游戏源文件／配置／正式资产哈希在最终实机验收后复核无变化，见 [源文件哈希](evidence/enterable-buildings/gameplay-source-hashes.json)。

游戏包位于本工作树 `Artifacts/Windows/Development/CityCourier.exe` 和 `Artifacts/Windows/Shipping/CityCourier.exe`；不要只复制 exe，需保留整个目录及字体许可。

门状态只保留本次会话，存档仍为 v2。本次不增加电梯、钥匙、住户 AI、生命值／战斗经济或多人功能。
