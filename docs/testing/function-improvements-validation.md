# 功能完善版验证（2026-10-18）

范围：操作失败反馈、驿站停车距离校验、候选订单距离与预览、独立驿站／找车导航、最近 20 单历史与版本 1 存档迁移。功能工作树为 `D:/eleme/.worktrees/city-courier`，美术工作树及二进制资产未修改。

## 自动化与界面

- Editor 编译成功；`Automation-20261018-125630-84c4d057/Report/index.json`：48/48，包含原有回归及新增行为、迁移、历史和原生界面测试。报告拒绝零测试、未执行或失败项。
- 版本 1 二进制来自升级前实际序列化器，见 `Source/CityCourier/Tests/Fixtures/ProfileV1.md`。覆盖旧主文件、旧备份、首次迁移写入失败、重试及未来版本保留，未使用新版程序伪造旧格式。
- 历史覆盖准时、迟到、超时、损毁、取消、救援中止、重复结算／保存、21 条裁剪、最长 UTF-16 名称、代理对边界、满容量序列化及非法记录。
- `FunctionUI-Editor-04`：80.084 秒通过。1920×1080 与 1280×720 实际截图确认返回按钮、中文、长名称、多行收入和 20 条历史滚动可用。368 个已使用中文字符通过字体覆盖检查。
- 前三个界面诊断运行不算通过：首次发现地图返回按钮被绘制层遮挡、长名称布局重叠、验证脚本同帧调整分辨率并截图导致 RHI 崩溃；后续发现移动交通被当作路由障碍。分别调整绘制层、明确换行宽度、将测试脚本调整分辨率与截图分开，并添加路由交通回归测试 RED→GREEN。建筑遮挡检查仍保留。

## 测试方法与边界

`-CourierValidation` 仅 Development opt-in 可用，Shipping 不生成验证驱动。所有运行使用单独 `-UserDir`，不读写玩家真实进度。

- `FeatureUI` 调用实际手机／菜单按钮与控制器方法；为验证高速下车、远程充电拒绝而移动测试角色／车辆；以 20 倍世界时间验证历史页超时结算；另注入 19 条有效完成快照验证容量与长名称。共 20 条、38000 分包含人工夹具，**不作为真实配送收入或连续游玩证明**。测试将自己的存档设为只读以触发真实保存失败，再恢复并通过菜单重试。
- `FeatureReload` 重开同一隔离存档，检查 20 条记录、38000 分、完成 19／失败 1，逐条校验且无活动订单。
- `LegacyCheck` 把真实旧格式夹具复制到新的隔离目录，验证余额 12345、累计收入 30000、完成 15／失败 3、升级 1/2/1、音量 0.3、画质 2 及空历史。
- `FeatureRoute` 使用正常移动、碰撞、取餐、交付、充电和计时接口：预览后接单、绕行驿站、找车并骑行送达。没有传送、注入收入或加速计时。`FeatureRouteReload` 检查真实订单记录和余额在重启后相等。

旧首版 22 分钟／8 单和 180 秒性能采样仍见 [首版发行记录](release-checklist.md)，不代表本次功能更新重新测量了性能。本次未重新做音频试听或长时性能验收。

## 打包与交付

| 检查 | 实际证据（均在 `Saved/Validation`） |
| --- | --- |
| Development 包 | `Package-Development-20261018-125721-b46c99f1`，ExitCode=0 |
| Shipping 包 | `Package-Shipping-20261018-125903-a678c136`，ExitCode=0；完整目录 703818448 字节，34 个文件 |
| 连续取送餐 | `FunctionRoute-Packaged-01`，68.756 秒，255.96 米，完成 1／失败 0，收入 1600 分；实际订单耗时 51 秒，承诺 147 秒 |
| 真实订单重载 | `FunctionRoute-Reload-01`，余额 1600 分、完成 1／失败 0、单条历史一致，无活动订单 |
| 打包 UI／故障检查 | `FunctionUI-Packaged-01`，77.323 秒；两种分辨率、预览、导航、历史超时、保存失败／重试通过，20 条记录／38000 分 |
| 满容量重载 | `FunctionUI-Reload-01`，20 条历史逐条有效，余额／累计计数一致，无活动订单 |
| 旧存档实机迁移 | `FunctionLegacy-Packaged-01`，旧余额、累计、升级、设置全部保留，新增历史为空 |
| Shipping 键鼠 | `FunctionShipping-Physical-01`：实际 Tab 打开／关闭手机，鼠标预览／地图返回／历史条目，M 从预览返回接单页，Esc 从历史／地图进入暂停，恢复后无旧子页，双确认正常退出；读取真实路线测试存档的副本，金额 16 元、收入明细正确 |

上述打包检查的 `error` 均为空；相关 JSON 和实际截图已归档至 [evidence/function-improvements](evidence/function-improvements)。Shipping 客户区实际为 1920×1080，外框截图 1944×1139；720p 检查来自 Development 实机，未把启动参数误当作实际分辨率。

本次代码和可运行目录保留在功能分支，不自动合并或推送。独立整体审查结果随后补入本记录。

复现 Development 检查时使用完整包中的 `CityCourier/Binaries/Win64/CityCourier.exe`，参数示例：

```powershell
$run='D:/eleme/.worktrees/city-courier/Saved/Validation/MyFeatureRun'
& ./Artifacts/Windows/Development/CityCourier/Binaries/Win64/CityCourier.exe -windowed -unattended -CourierValidation -CourierValidationMode=FeatureRoute "-CourierValidationOutput=$run" "-UserDir=$run/User"
```

每次首次检查使用新目录；重载模式保留原检查的 `-UserDir` 并改用新的输出目录。不要将故障检查指向真实玩家目录。`result.json` 的 `error` 必须为空，并核对预期金额和计数，不能只看进程退出码。
