# 街头骑手 / 南城

Windows 单人 3D 外卖配送游戏，基于 Unreal Engine 5.6.1。首版街区约 600×600 米，包含 4 家商户、8 个收餐点、两条近路和骑手驿站。

![实际打包版游戏画面](docs/testing/evidence/task12/gameplay.png)

## 启动

正式版本入口：`Artifacts/Windows/Shipping/CityCourier.exe`。保留整个 Shipping 文件夹及其中的 `CityCourier`、`Engine` 目录，不能单独复制根目录的小型启动 exe。

本机也可双击 `D:\eleme\街头骑手.lnk`。工程与完整包保存在 `D:\eleme\.worktrees\city-courier`；移动工程后需重新创建该快捷方式。Shipping 完整目录约671 MiB，附带 `开始游戏.txt` 和字体许可。

开发版本位于 `Artifacts/Windows/Development/CityCourier.exe`。打开工程使用 `CityCourier.uproject`，本机引擎位于 `D:\sdk\UE_5.6`。

## 第一单

1. 按 **Tab** 打开手机，选择一个候选订单。
2. 靠近电动车按 **E** 上车，沿小地图导航前往商户；按 **M** 查看完整街区和路线。
3. 按空格刹停，以不超过 3 km/h 的速度按 **E** 下车。步行到商户入口 2 米以内，再按 **E** 取餐。
4. 导航会切换到收餐点。到达后下车、步行靠近入口并按 **E** 交付，查看结算。
5. 回到骑手驿站可免费充电和购买升级。

## 操作

| 按键 | 功能 |
| --- | --- |
| W A S D | 步行 / 驾驶；停车后 S 倒车 |
| 鼠标移动 | 转动镜头 |
| Shift | 步行奔跑 |
| E | 交互、上下车、取餐、交付 |
| 空格 | 电动车刹车 |
| Tab | 手机、接单、取消订单和没电救援 |
| M | 完整地图 |
| Esc | 暂停菜单 / 返回游戏 |
| 长按 R 两秒 | 脱困；有订单时增加 5 秒配送用时 |

同时只配送一单。基础车速 36 km/h、续航 3 公里；车辆升级分别改善动力、转向和电池。电量按实际移动距离消耗，撞击会损伤餐品。超时后有 60 秒宽限期，期间送达按 60% 比例结算；失败不扣除已有余额。手机和地图不会暂停配送计时，Esc 暂停菜单会暂停世界。

驿站服务要求下车、靠近入口且车辆停稳。充电需完整等待 5 秒，提前离开或关闭服务会取消。远离驿站且电量为零时，手机可确认救援；会终止当前订单，回到驿站并充满电。

## 设置与进度

Esc 菜单提供总音量和低 / 中 / 高画质。中档使用软件 Lumen，1080p 输出采用 83% 内部分辨率重建；高档使用 100% 内部分辨率，硬件光追默认关闭。

常规打包版进度位于 `%LOCALAPPDATA%\CityCourier\Saved\SaveGames`；指定 `-UserDir` 的验证进程使用各自独立目录。主文件 `CourierProfile.sav`，最近一次有效备份为 `CourierProfile.bak`。余额、升级和设置自动保存，活动订单不跨游戏重启保留。

保存失败时，界面会提示“本次进度未保存”；恢复磁盘空间或写入权限后点“重试保存”。存档损坏会先尝试有效备份，原损坏文件会保留。如果主文件和备份均不可恢复，界面提供两次确认的“创建新进度”，会将余额和升级归零。需要手动备份时先关闭游戏，再复制整个 SaveGames 文件夹。

退出游戏需要确认；存在未保存进度时会再次尝试保存并提示问题。

## 工程与验证

```powershell
./scripts/Build.ps1 -Target Editor -EngineRoot D:/sdk/UE_5.6
./scripts/Test.ps1 -Filter CityCourier -EngineRoot D:/sdk/UE_5.6
./scripts/Package.ps1 -Configuration Shipping -EngineRoot D:/sdk/UE_5.6
```

设计和实施记录在 `docs/superpowers`；美术源文件和音源在 `AssetsSource/CityCourier`，制作流程见 [美术制作说明](docs/art/authoring.md)，来源见 [素材清单](docs/art/asset-manifest.csv)。中文字体保留 Noto CJK 的 OFL 许可证。

最终验收与性能实测见 [验收记录](docs/testing/acceptance.md)、[发行检查](docs/testing/release-checklist.md) 和 [性能报告](docs/testing/performance.md)。运行时不需要 Python、Blender 或 UE 编辑器。

## 首版范围

当前使用原创简化模型和固定傍晚街区；自由步行、骑行和配送循环已实现，画面规模与精细度不等同于 GTA 成品。背景交通为最多 6 辆车、12 名行人的有限巡行和避障。首版不包含多人、建筑内部探索、警察、战斗、真实地图平台、真实支付、天气或昼夜循环。

## Integrated art preview

main includes gameplay and updated art. The game starts in L_CourierCity; the editor opens L_CourierArtPreview. See [art preview guide](docs/art/preview-guide.md) for sources and rebuilding. Existing packages in the original worktree predate this merge. Build and package main to use the merged version.
